/*
 * Schedule Fuzzing Module
 *
 * Implements controlled randomness in scheduling decisions to find
 * race conditions. Uses xorshift128+ PRNG for deterministic
 * pseudo-randomness when given a seed.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "runtime_state.h"

/*
 * xorshift128+ PRNG constants
 * These values give good statistical properties for our use case
 */
#define XSHIFT_ROT1 23
#define XSHIFT_ROT2 17
#define XSHIFT_ROT3 26
#define XSHIFT_PLUS 0x9E3779B97F4A7C15ULL

/*
 * Default perturbation rate: 10% chance of perturbing a decision
 */
#define GTH_FUZZ_DEFAULT_PERTURBATION_RATE 10U

/*
 * Number of scheduling decisions to skip before allowing perturbations
 * This gives threads time to initialize before we start fuzzing
 */
#define GTH_FUZZ_WARMUP_DECISIONS 5U

/*
 * Initialize xorshift128+ state from seed
 */
static void gth_fuzz_seed(gth_fuzz_state_t *fuzz, uint64_t seed)
{
    /*
     * Initialize state using splitmix64 algorithm
     * This provides good initial mixing of the seed
     */
    uint64_t z = seed + XSHIFT_PLUS;
    fuzz->state[0] = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    fuzz->state[1] = fuzz->state[0] + XSHIFT_PLUS;

    /* Additional mixing */
    fuzz->state[0] ^= (z >> 33);
}

/*
 * Generate next random value using xorshift128+
 */
static uint64_t gth_fuzz_xorshift128plus(gth_fuzz_state_t *fuzz)
{
    uint64_t s0 = fuzz->state[0];
    uint64_t s1 = fuzz->state[1];

    /* xorshift128+ algorithm */
    s1 ^= s1 << XSHIFT_ROT1;
    fuzz->state[0] = s1;
    fuzz->state[1] =
        (fuzz->state[1] ^ (fuzz->state[1] >> XSHIFT_ROT3)) ^ (s1 ^ (s1 >> XSHIFT_ROT2));

    return fuzz->state[1] + s0;
}

/*
 * Initialize fuzzing subsystem
 */
gth_status_t gth_fuzz_init(uint64_t seed)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (!state->initialized)
    {
        return GTH_ESTATE;
    }

    if (state->fuzz != NULL)
    {
        return GTH_ESTATE; /* Already fuzzing */
    }

    state->fuzz = (gth_fuzz_state_t *)malloc(sizeof(gth_fuzz_state_t));
    if (state->fuzz == NULL)
    {
        return GTH_ENOMEM;
    }

    state->fuzz->seed = seed;
    state->fuzz->decision_count = 0;
    state->fuzz->perturbation_rate = GTH_FUZZ_DEFAULT_PERTURBATION_RATE;

    gth_fuzz_seed(state->fuzz, seed);

    return GTH_OK;
}

/*
 * Cleanup fuzzing subsystem
 */
void gth_fuzz_cleanup(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || state->fuzz == NULL)
    {
        return;
    }

    free(state->fuzz);
    state->fuzz = NULL;
}

/*
 * Generate random 64-bit value
 */
uint64_t gth_fuzz_random(void)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || state->fuzz == NULL)
    {
        return 0;
    }

    return gth_fuzz_xorshift128plus(state->fuzz);
}

/*
 * Generate random value in range [0, max)
 */
static uint64_t gth_fuzz_random_range(gth_fuzz_state_t *fuzz, uint64_t max)
{
    if (fuzz == NULL || max == 0)
    {
        return 0;
    }

    /* Rejection sampling to avoid modulo bias */
    uint64_t mask = max - 1;
    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8;
    mask |= mask >> 16;
    mask |= mask >> 32;

    uint64_t result;
    do
    {
        result = gth_fuzz_xorshift128plus(fuzz) & mask;
    } while (result >= max);

    return result;
}

/*
 * Check if we should perturb this scheduling decision
 */
int gth_fuzz_should_perturb(void)
{
    gth_runtime_state_t *state = gth_runtime_state();
    gth_fuzz_state_t *fuzz;

    if (state == NULL || state->fuzz == NULL)
    {
        return 0;
    }

    fuzz = state->fuzz;
    fuzz->decision_count++;

    /* Warmup period: no perturbations for first N decisions */
    if (fuzz->decision_count <= GTH_FUZZ_WARMUP_DECISIONS)
    {
        return 0;
    }

    /* Generate random value 0-99 and compare to rate */
    uint64_t roll = gth_fuzz_xorshift128plus(fuzz) % 100;

    return roll < fuzz->perturbation_rate;
}

/*
 * Count ready threads in the system
 */
static size_t gth_fuzz_count_ready_threads(gth_runtime_state_t *state)
{
    size_t count = 0;

    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        if (state->threads[i].state == GTH_THREAD_READY)
        {
            count++;
        }
    }

    return count;
}

/*
 * Select a random ready thread (other than the given one)
 */
static gth_thread_record_t *gth_fuzz_pick_alternative(gth_runtime_state_t *state,
                                                      gth_thread_record_t *exclude)
{
    size_t ready_count = gth_fuzz_count_ready_threads(state);
    gth_thread_record_t *result = NULL;

    if (ready_count <= 1)
    {
        return exclude; /* No alternative available */
    }

    /* Pick random index among ready threads */
    size_t target_idx = (size_t)gth_fuzz_random_range(state->fuzz, (uint64_t)ready_count);
    size_t current_idx = 0;

    for (size_t i = 0; i < GTH_MAX_THREADS; ++i)
    {
        if (state->threads[i].state != GTH_THREAD_READY)
        {
            continue;
        }

        /* Skip the excluded thread when counting */
        if (&state->threads[i] == exclude)
        {
            continue;
        }

        if (current_idx == target_idx)
        {
            result = &state->threads[i];
            break;
        }

        current_idx++;
    }

    return result != NULL ? result : exclude;
}

/*
 * Fuzzing-aware thread selection
 *
 * Normal_choice is what the scheduler would normally pick.
 * We may return a different thread to perturb the schedule.
 */
gth_status_t gth_fuzz_get_stats(gth_fuzz_stats_t *out_stats)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (out_stats == NULL)
    {
        return GTH_EINVAL;
    }

    if (state == NULL || !state->initialized)
    {
        return GTH_ESTATE;
    }

    if (state->fuzz == NULL)
    {
        return GTH_ESTATE; /* Fuzzing not active */
    }

    out_stats->decision_count = state->fuzz->decision_count;
    out_stats->perturbation_rate = state->fuzz->perturbation_rate;

    return GTH_OK;
}

gth_status_t gth_fuzz_set_rate(uint32_t rate)
{
    gth_runtime_state_t *state = gth_runtime_state();

    if (state == NULL || !state->initialized)
    {
        return GTH_ESTATE;
    }

    if (state->fuzz == NULL)
    {
        return GTH_ESTATE; /* Fuzzing not active */
    }

    if (rate > 100U)
    {
        return GTH_EINVAL;
    }

    state->fuzz->perturbation_rate = rate;

    return GTH_OK;
}

gth_thread_record_t *gth_fuzz_pick_thread(gth_runtime_state_t *state,
                                          gth_thread_record_t *normal_choice)
{
    gth_fuzz_state_t *fuzz;

    if (state == NULL || normal_choice == NULL)
    {
        return NULL;
    }

    fuzz = state->fuzz;
    if (fuzz == NULL)
    {
        return normal_choice; /* Not fuzzing, use normal choice */
    }

    /* Check if we should perturb this decision */
    if (!gth_fuzz_should_perturb())
    {
        return normal_choice;
    }

    /* Perturbation strategy: pick a different ready thread */
    /* Other strategies could include: */
    /* - Invert priority order */
    /* - Random yield injection */
    /* - Delay thread unblocking */

    return gth_fuzz_pick_alternative(state, normal_choice);
}
