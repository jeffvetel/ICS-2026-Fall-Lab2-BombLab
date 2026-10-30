# BombLab 关键逻辑摘录

本文只保留每关的最终比较、关键函数调用和核心计算。完整汇编、链表数据及测试记录见
[bomb_analysis.md](bomb_analysis.md)。

## 第 1 关：函数调用

关键调用和组合：

```asm
call p1_step0
call p1_step1
call p1_step2
xor  eax,ecx
```

三个 helper 的核心计算：

```asm
; p1_step0
xor esi,0xd0dd
lea eax,[rsi+rdi*2]

; p1_step1
lea eax,[rsi+0xf31c]
xor eax,edi

; p1_step2
lea eax,[rdi+rdi*2]
add esi,0x7132
xor eax,esi
```

处理逻辑：

```text
low   = challenge 的低 32 位
mid   = (challenge >> 16) 的低 32 位
high  = challenge 的高 32 位

step0 = low * 2 + (high XOR 0xd0dd)
step1 = (mid + 0xf31c) XOR high
step2 = (step1 * 3) XOR (low + 0x7132)

values[0] = step2 XOR step0
count = 1
```

最终检查：

```asm
cmp DWORD PTR [rbp-0x14],eax
sete al
```

## 第 2 关：数组与递推

输入为 6 个整数，首项检查：

```asm
shr rdx,0x3
and edx,0x7f
add edx,0xa
cmp eax,edx
```

循环中的核心计算：

```asm
lea ecx,[rdx*4]
add eax,edx
lea eax,[rax+rax*4]
shr rsi,cl
and esi,0x1f
sub eax,esi
```

处理逻辑：

```text
values[0] = ((challenge >> 3) & 0x7f) + 10

对 i = 1..5：
    bits = (challenge >> (4 * i)) & 0x1f
    values[i] = (values[i - 1] + i) * 5 - bits

count = 6
```

循环每次将当前输入与计算值比较，六项全部相等才通过。

## 第 3 关：跳转表

入口先确定操作码和分支参数：

```asm
and edi,0x7
shr rax,0x3
shr rsi,0x20
xor esi,eax
call p3_dispatch
cmp DWORD PTR [rbp-0x14],eax
```

处理逻辑：

```text
n = challenge & 0x7
x = (challenge >> 3 的低 32 位) XOR challenge 高 32 位
result = 按 n 选择对应计算公式

values[0] = n
values[1] = result
count = 2
```

跳转表映射和公式：

| `n` | 分支 | 核心计算 |
|---:|---|---|
| 0 | `p3_case_5` | `(x ^ 0x5939) + (x >> 2)` |
| 1 | `p3_case_4` | `(x * 4) ^ 0x732e` |
| 2 | `p3_case_6` | `x * 5 + 0x313d0` |
| 3 | `p3_case_1` | `x * 5 + 0x2c66e` |
| 4 | `p3_case_2` | `(x ^ 0x8d39) + (x >> 2)` |
| 5 | `p3_case_7` | `x * 5 + 0x1abd` |
| 6 | `p3_case_3` | `(x * 4) ^ 0x887c` |
| 7 | `p3_case_0` | `x * 5 + 0x45d0` |

## 第 4 关：递归

外层构造递归参数：

```asm
and edi,0x7f
xor esi,esi
mov edx,0x7f
shr rcx,0x7
shr rax,0x20
xor ecx,eax
call p4_walk
cmp DWORD PTR [rbp-0x14],eax
```

递归函数核心：

```asm
sub edi,esi
shr edi,1
add edi,esi
cmp edi,eax
je 4023b0
call p4_walk
lea eax,[rax+rax*4]
xor eax,ebx
xor eax,0x3
```

处理逻辑：

```text
target = challenge & 0x7f
salt = (challenge >> 7 的低 32 位) XOR challenge 高 32 位

p4_walk(target, 0, 127, salt):
    middle = low + (high - low) / 2

    target == middle:
        return (middle + 0xe1) ^ salt

    target < middle:
        return
            p4_walk(target, low, middle - 1, salt) * 5
            ^ (middle + salt)
            ^ 0xa

    target > middle:
        return
            p4_walk(target, middle + 1, high, salt) * 5
            ^ (middle + salt)
            ^ 0x3

values[0] = p4_walk(...)
count = 1
```

## 第 5 关：结构体与链表

关键数据访问：

```asm
and eax,0x7
mov rdx,QWORD PTR [rax*8+0x405b60]
mov esi,DWORD PTR [rdx+0x4]
add eax,DWORD PTR [rdx]
mov rdx,QWORD PTR [rdx+0x8]
```

结构布局：

```text
offset 0：value
offset 4：tag
offset 8：next
```

处理逻辑：

```text
head_index = challenge & 0x7
node = heads[head_index]
extra_steps = (challenge >> 3) & 0x3

mixed =
    (challenge >> 6 的低 32 位)
    XOR challenge 高 32 位

accumulator =
    ((mixed ^ 0xa582) * 3 + node.value) ^ node.tag
computed_tag = node.tag

重复 extra_steps 次：
    node = node.next
    accumulator = accumulator * 3 + node.value
    computed_tag = node.tag
    accumulator = accumulator ^ node.tag

values[0] = accumulator
values[1] = computed_tag
count = 2
```

外层最终依次比较累计值和最后节点的 tag。

## 第 6 关：函数指针表

`p6_dispatch` 的关键间接调用：

```asm
shr rdi,0x6
shr rax,0x20
xor edi,eax
and ebx,0x38
and eax,0x7
call QWORD PTR [rax*8+0x405e00]
xor eax,0xc31b
mov edi,eax
call QWORD PTR [rbx+0x405e00]
add eax,0xb338
```

处理逻辑：

```text
payload =
    (challenge >> 6 的低 32 位)
    XOR challenge 高 32 位

first_index  = challenge & 0x7
second_index = (challenge >> 3) & 0x7

first_result = operations[first_index](payload)
first_result = first_result XOR 0xc31b
second_result = operations[second_index](first_result)

values[0] = second_result + 0xb338
count = 1
```

函数指针表：

```text
0 → p6_op_1
1 → p6_op_2
2 → p6_op_1
3 → p6_op_5
4 → p6_op_4
5 → p6_op_3
6 → p6_op_4
7 → p6_op_0
```

各操作函数：

```text
p6_op_0(x) = ((x << 7) + 0x6d54) ^ (x * 3)
p6_op_1(x) = rotl32(x, 1) ^ 0xe6cc
p6_op_2(x) = rotr32(x, 7)
p6_op_3(x) = (x + 0x499d) ^ (x >> 5)
p6_op_4(x) = (x ^ 0xb913) + (x << 7)
p6_op_5(x) = ((x >> 2) ^ x) + 0xbda6
```

其中循环移位为：

```text
rotl32(x, 1) = (x << 1) | (x >> 31)
rotr32(x, 7) = (x >> 7) | (x << 25)
```

## Honor（已分析，尚未实现验证）

输入布局：

```text
第 0～31 字节：前缀
第 32～35 字节：OPEN
第 36 字节开始：3 个无符号整数
```

`secret_gate` 的关键调用：

```asm
cmp DWORD PTR [rbp-0xb0],0x4e45504f
lea rdi,[rbp-0xac]
call secret_phase
```

三个整数的计算逻辑：

```text
challenge_fold =
    (uint32_t)challenge XOR (uint32_t)(challenge >> 32)

context_fold =
    (uint32_t)context XOR (uint32_t)(context >> 32)

selector = (uint32_t)context & 0x3

secret_mix(x) =
    ((x >> 7) ^ 0xf98d) + (x ^ 0x4558)

first = selector

second =
    secret_mix(challenge_fold ^ context_fold)
    ^
    (
        selector * 0x4558
        ^ (uint32_t)(context >> 16)
    )

third =
    second
    ^
    (
        context_fold
        + selector
        + 0xf98d
        + (challenge_fold >> 7)
    )
```

最终输出行需要满足：

```text
[32 字节前缀]OPEN [first] [second] [third]
```

## 验证状态

| 关卡 | 公开测试 | 非公开 seed | 截图 |
|---|---|---|---|
| 1 | 10/10 | 2025 | 已保存 |
| 2 | 10/10 | 2027 | 已保存 |
| 3 | 10/10 | 2025 | 已保存 |
| 4 | 10/10 | 2027 | 已保存 |
| 5 | 10/10 | 2025 | 已保存 |
| 6 | 10/10 | 2027 | 已保存 |
| Honor | 待实现 | 待验证 | 待保存 |

