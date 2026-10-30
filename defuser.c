#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/*
 * 把 values 中的 count 个整数写成一行“空格分隔的无符号十进制整数”，存入 out。
 * 成功返回 0；count 为 0（还没有实现）、snprintf 出错或结果被截断时返回非零。
 */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    //(void)challenge;
    // TODO: 根据 phase_01 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    uint32_t low32 = (uint32_t)challenge;
    uint32_t middle32 = (uint32_t)(challenge >> 16);
    uint32_t high32 = (uint32_t)(challenge >> 32);

    uint32_t step0_result = low32 * UINT32_C(2) + (high32 ^ UINT32_C(0xd0dd));
    uint32_t step1_result = (middle32 + UINT32_C(0xf31c)) ^ high32;
    uint32_t step2_result = (step1_result * UINT32_C(3)) ^ (low32 + UINT32_C(0x7132));

    values[0] = step2_result ^ step0_result;
    count = 1;

    return write_answer(out, cap, values, count);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    //(void)challenge;
    // TODO: 根据 phase_02 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    values[0] = ((challenge >> 3) & 0x7f) + UINT32_C(10);
    for (uint32_t i = 1; i <= 5; i++) {
        uint32_t challenge_bits = (uint32_t)( (challenge >> (4 * i)) & UINT64_C(0x1f) );
        values[i] = (values[i - 1] + i) * 5 - challenge_bits;
    }
    count = 6;

    return write_answer(out, cap, values, count);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    //(void)challenge;
    // TODO: 根据 phase_03 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    uint32_t n = (uint32_t)(challenge & UINT64_C(0x7));
    uint32_t x = (uint32_t)(challenge >> 3) ^ (uint32_t)(challenge >> 32);

    uint32_t result;

    switch (n) {
        case 0:
            result = (x ^ UINT32_C(0x5939)) + (x >> 2);
            break;
        case 1:
            result = (x * 4) ^ UINT32_C(0x732e);
            break;
        case 2:
            result = x * 5 + UINT32_C(0x313d0);
            break;
        case 3:
            result = x * 5 + UINT32_C(0x2c66e);
            break;
        case 4:
            result = (x ^ UINT32_C(0x8d39)) + (x >> 2);
            break;
        case 5:
            result = x * 5 + UINT32_C(0x1abd);
            break;
        case 6:
            result = (x * 4) ^ UINT32_C(0x887c);
            break;
        case 7:
            result = x * 5 + UINT32_C(0x45d0);
            break;
        default:
            return 1;
    }

    values[0] = n;
    values[1] = result;
    count = 2;

    return write_answer(out, cap, values, count);
}

static uint32_t p4_walk(uint32_t target, uint32_t low, uint32_t high, uint32_t salt) {
    uint32_t middle = low + (high - low) / UINT32_C(2);

    if (target == middle)   return (middle + UINT32_C(0xe1)) ^ salt;
    else if (target < middle) {
        uint32_t result = p4_walk(target, low, middle - UINT32_C(1), salt);
        return (result * 5) ^ (middle + salt) ^ UINT32_C(0xa);
    } else {
        uint32_t result = p4_walk(target, middle + UINT32_C(1), high, salt);
        return (result * 5) ^ (middle + salt) ^ UINT32_C(0x3);
    }   
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    //(void)challenge;
    // TODO: 根据 phase_04 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    uint32_t target = (uint32_t)(challenge & UINT64_C(0x7f));
    uint32_t low = UINT32_C(0), high = UINT32_C(0x7f);
    uint32_t salt = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);

    values[0] = p4_walk(target, low, high, salt);
    count = 1;

    return write_answer(out, cap, values, count);
}

struct Node {
    uint32_t value;
    uint32_t tag;
    int next_index;
};

static const int heads[8] = {
    1, 11, 3, 8, 2, 5, 6, 7
};
static const struct Node nodes[12] = {
    {60077, 285,  10},
    {22543, 1544, 11},
    {47206, 1888,  5},
    {13497, 2632,  8},
    {4958,   895, -1},
    {18360,   31,  6},
    {51270, 2914,  7},
    {7752,   564,  0},
    {23209, 1335,  2},
    {13448, 1125,  4},
    {51348, 2430,  9},
    {29473, 2091,  3}
};

static uint32_t p5_traverse(uint64_t challenge, uint32_t *computed_tag) {
    uint32_t head_index = (uint32_t)(challenge & UINT32_C(0x7));
    int node_index = heads[head_index];

    uint32_t low_part = (uint32_t)(challenge >> 6);
    uint32_t high_part = (uint32_t)(challenge >> 32);
    uint32_t mixed = low_part ^ high_part;

    struct Node node = nodes[node_index];
    uint32_t accumulator = ((mixed ^ UINT32_C(0xa582)) * UINT32_C(3) + node.value) ^ node.tag;
    *computed_tag = node.tag;

    uint32_t extra_steps = (uint32_t)((challenge >> 3) & UINT64_C(0x3));
    for (uint32_t i = 0; i < extra_steps; i++) {
        node_index = nodes[node_index].next_index;
        node = nodes[node_index];

        accumulator = accumulator * UINT32_C(3) + node.value;
        *computed_tag = node.tag;
        accumulator = accumulator ^ node.tag;
    }

    return accumulator;
}

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    //(void)challenge;
    // TODO: 根据 phase_05 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    uint32_t actual_tag = 0;
    uint32_t actual_accumulator = p5_traverse(challenge, &actual_tag);

    values[0] = actual_accumulator;
    values[1] = actual_tag;
    count = 2;

    return write_answer(out, cap, values, count);
}

static uint32_t p6_op_0(uint32_t x) {
    return ((x << 7) + UINT32_C(0x6d54)) ^ (x * 3);
}
static uint32_t p6_op_1(uint32_t x) {
    return ((x << 1) | (x >> 31)) ^ UINT32_C(0xe6cc);
}
static uint32_t p6_op_2(uint32_t x) {
    return (x >> 7) | (x << 25);
}
static uint32_t p6_op_3(uint32_t x) {
    return (x + UINT32_C(0x499d)) ^ (x >> 5);
}
static uint32_t p6_op_4(uint32_t x) {
    return (x ^ UINT32_C(0xb913)) + (x << 7);
}
static uint32_t p6_op_5(uint32_t x) {
    return ((x >> 2) ^ x) + UINT32_C(0xbda6);
}

typedef uint32_t (*p6_operation)(uint32_t);
static const p6_operation operations[8] = {
    p6_op_1,
    p6_op_2,
    p6_op_1,
    p6_op_5,
    p6_op_4,
    p6_op_3,
    p6_op_4,
    p6_op_0
};

static uint32_t p6_dispatch(uint64_t challenge) {
    uint32_t payload = (uint32_t)((challenge >> 6)) ^ (uint32_t)(challenge >> 32);
    unsigned first_index = (unsigned)(challenge & UINT64_C(7));
    unsigned second_index = (unsigned)((challenge >> 3) & UINT64_C(7));

    uint32_t first_result = operations[first_index](payload);
    first_result = first_result ^ UINT32_C(0xc31b);

    uint32_t second_result = operations[second_index](first_result);
    return second_result + UINT32_C(0xb338);
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    //(void)challenge;
    // TODO: 根据 phase_06 的汇编，用 challenge 算出口令中的各个整数，依次存入 values，并把 count 改为整数个数。
    values[0] = p6_dispatch(challenge);
    count = 1;

    return write_answer(out, cap, values, count);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    (void)challenge;
    (void)context;
    (void)out;
    (void)cap;
    // TODO（选做）: 根据 secret_gate、secret_copy 和 secret_phase 的汇编构造完整的第七行，写入 out 后返回 0。
    // 第七行不一定只有整数，可以直接用 snprintf 写入；还没完成时保持返回 1，测试会显示 not attempted。
    return 1;
}
