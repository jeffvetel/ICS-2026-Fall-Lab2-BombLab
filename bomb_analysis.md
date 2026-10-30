# BombLab 分析记录

本文档用于汇总各阶段的关键汇编、寄存器含义、恢复出的计算关系、验证命令和结果，
作为实验报告的原始分析材料。

当前 C 实现中的变量名、结构体字段和辅助函数名以 `defuser.c` 为准。反汇编中出现的
`p1_compute`、`p3_dispatch`、`p5_traverse` 等名称是炸弹自身的函数名，用于说明调用链；
求解器可以根据当前实现使用不同的内部变量名，但本文记录已与 `defuser.c` 对齐。

## 第 1 关

### 关键汇编

#### `p1_compute`

```asm
0000000000402050 <p1_compute>:
  402054:       push   rbp
  402055:       mov    rax,rdi
  402058:       mov    rdx,rdi
  40205b:       mov    r8d,edi
  40205e:       shr    rax,0x20
  402062:       shr    rdx,0x10
  402066:       mov    r9d,eax
  402069:       mov    esi,eax
  40206b:       mov    rbp,rsp
  40206e:       call   402020 <p1_step0>
  402073:       mov    rsi,rdx
  402076:       mov    edi,r9d
  402079:       mov    ecx,eax
  40207b:       call   402030 <p1_step1>
  402080:       mov    esi,r8d
  402083:       mov    edi,eax
  402085:       call   402040 <p1_step2>
  40208a:       pop    rbp
  40208b:       xor    eax,ecx
  40208d:       ret
```

#### `p1_step0`

```asm
0000000000402020 <p1_step0>:
  402024:       xor    esi,0xd0dd
  40202a:       lea    eax,[rsi+rdi*2]
  40202d:       ret
```

#### `p1_step1`

```asm
0000000000402030 <p1_step1>:
  402034:       lea    eax,[rsi+0xf31c]
  40203a:       xor    eax,edi
  40203c:       ret
```

#### `p1_step2`

```asm
0000000000402040 <p1_step2>:
  402044:       lea    eax,[rdi+rdi*2]
  402047:       add    esi,0x7132
  40204d:       xor    eax,esi
  40204f:       ret
```

### challenge 拆分

`p1_compute` 接收一个 64 位 challenge，并从中取出三个 32 位片段：

```c
uint32_t low32    = (uint32_t)challenge;
uint32_t middle32 = (uint32_t)(challenge >> 16);
uint32_t high32   = (uint32_t)(challenge >> 32);
```

含义：

- `low32`：challenge 的最低 32 位。
- `middle32`：challenge 先右移 16 位，再取结果的低 32 位。
- `high32`：challenge 的最高 32 位。

### 三个 helper 的数据流

#### `p1_step0`

调用前的参数：

```text
edi = low32
esi = high32
```

关键操作：

```text
esi = high32 XOR 0xd0dd
eax = esi + edi * 2
```

恢复出的关系：

```text
step0_result =
    low32 * 2 + (high32 XOR 0xd0dd)
```

#### `p1_step1`

调用前的参数：

```text
edi = high32
esi = middle32
```

关键操作：

```text
eax = esi + 0xf31c
eax = eax XOR edi
```

恢复出的关系：

```text
step1_result =
    (middle32 + 0xf31c) XOR high32
```

#### `p1_step2`

调用前的参数：

```text
edi = step1_result
esi = low32
```

关键操作：

```text
eax = edi * 3
esi = esi + 0x7132
eax = eax XOR esi
```

恢复出的关系：

```text
step2_result =
    (step1_result * 3) XOR (low32 + 0x7132)
```

#### 最终组合

`p1_step0` 的返回值曾在调用 `p1_step1` 前保存到 `ecx`。三个 helper 全部返回后，
`p1_compute` 执行：

```asm
xor eax,ecx
```

此时：

```text
eax = step2_result
ecx = step0_result
```

因此最终计算关系为：

```text
values[0] =
    step2_result XOR step0_result
```

当前 C 实现随后设置：

```c
values[0] = step2_result ^ step0_result;
count = 1;
```

`step1_result` 已经作为 `p1_step2` 的第一个参数参与计算，所以最终结果不再直接异或
`step1_result`。

### 完整数据流

```text
challenge
   |
   +-- low32 --------------------------> p1_step0 <-- high32
   |                                        |
   |                                    step0_result --------+
   |                                                         |
   +-- high32 --------------------------> p1_step1 <-- middle32
   |                                        |
   |                                    step1_result
   |                                        |
   +-- low32 ---------------------------> p1_step2
                                            |
                                       step2_result
                                            |
                                            XOR <-----------+
                                            |
                                      values[0]
```

### seed 42 的 GDB 验证

原始 challenge：

```text
0x28efe333b266f103
```

拆分结果：

```text
low32    = 0xb266f103
middle32 = 0xe333b266
high32   = 0x28efe333
```

三个中间结果：

```text
step0_result = 0x8dbd15f4
step1_result = 0xcbdb46b1
step2_result = 0xd1f6b626
```

最终结果：

```text
values[0] = 0xd1f6b626 XOR 0x8dbd15f4
          = 0x5c4ba3d2
          = 1548461010
```

GDB 在 `phase_01` 调用 `p1_compute` 后观察到：

```text
p/x $eax = 0x5c4ba3d2
p/u $eax = 1548461010
```

手算结果与 GDB 观察值一致。

### 测试记录

以下记录来自实现第 2 关之前对第 1 关的验证。

#### seed 42

直接使用 seed 42 时，求解器输出：

```text
1548461010
```

交给炸弹验证：

```bash
./defuser --seed 42 --phase 1 | ./bomb --seed 42 --phase 1
```

对应的 challenge：

```text
0x28efe333b266f103
```

炸弹输出：

```text
Phase 1 defused.
```

#### 公开测试

执行：

```bash
python3 public_test.py --phase 1
```

结果：

```text
Phase 1 seed 0: PASS
Phase 1 seed 1: PASS
Phase 1 seed 7: PASS
Phase 1 seed 42: PASS
Phase 1 seed 99: PASS
Phase 1 seed 123: PASS
Phase 1 seed 2026: PASS
Phase 1 seed 65535: PASS
Phase 1 seed 987654321: PASS
Phase 1 seed 18446744073709551615: PASS
Public tests: 10/10
```

#### 非公开 seed 验证

使用 seed `2025`：

```bash
./defuser --seed 2025 --phase 1 |
    ./bomb --seed 2025 --phase 1
```

对应的 challenge：

```text
0xfcb3a4d54e7a52c2
```

炸弹输出：

```text
Phase 1 defused.
```

#### 完成状态

- [x] `make` 编译成功。
- [x] seed 42 单关运行成功。
- [x] `python3 public_test.py --phase 1` 通过，结果为 `10/10`。
- [x] 非公开 seed 2025 验证成功。
- [x] 第 1 关运行成功截图已保存（具体文件名待补充）。

## 第 2 关

### 关键汇编

第 2 关的计算直接写在 `phase_02` 中，没有调用专门的 helper 函数。

```asm
00000000004020f0 <phase_02>:
  4020f0:       endbr64
  4020f4:       push   rbp
  4020f5:       mov    rbp,rsp
  4020f8:       push   r13
  4020fa:       mov    r13,rsi
  4020fd:       mov    esi,0x6
  402102:       push   r12
  402104:       mov    r12,rdi
  402107:       push   rbx
  402108:       sub    rsp,0x28
  40210c:       call   402ca0 <valid_u32_fields>
  402111:       test   al,al
  402113:       jne    402128 <phase_02+0x38>
  402115:       xor    ebx,ebx
  402117:       lea    rsp,[rbp-0x18]
  40211b:       mov    eax,ebx
  40211d:       pop    rbx
  40211e:       pop    r12
  402120:       pop    r13
  402122:       pop    rbp
  402123:       ret
  402128:       mov    ebx,eax
  40212a:       lea    rax,[rbp-0x2c]
  40212e:       lea    rcx,[rbp-0x3c]
  402132:       mov    esi,0x40317a
  402137:       push   rax
  402138:       lea    rax,[rbp-0x30]
  40213c:       lea    rdx,[rbp-0x40]
  402140:       mov    rdi,r12
  402143:       push   rax
  402144:       lea    r9,[rbp-0x34]
  402148:       lea    r8,[rbp-0x38]
  40214c:       xor    eax,eax
  40214e:       call   4011e0 <__isoc99_sscanf@plt>
  402153:       pop    rdx
  402154:       pop    rcx
  402155:       cmp    eax,0x6
  402158:       jne    402115 <phase_02+0x25>
  40215a:       mov    rdx,r13
  40215d:       mov    eax,DWORD PTR [rbp-0x40]
  402160:       shr    rdx,0x3
  402164:       and    edx,0x7f
  402167:       add    edx,0xa
  40216a:       cmp    eax,edx
  40216c:       jne    402115 <phase_02+0x25>
  40216e:       mov    edx,0x1
  402173:       lea    ecx,[rdx*4+0x0]
  40217a:       mov    rsi,r13
  40217d:       add    eax,edx
  40217f:       shr    rsi,cl
  402182:       lea    eax,[rax+rax*4]
  402185:       and    esi,0x1f
  402188:       sub    eax,esi
  40218a:       mov    ecx,eax
  40218c:       mov    eax,DWORD PTR [rbp+rdx*4-0x40]
  402190:       cmp    eax,ecx
  402192:       jne    402115 <phase_02+0x25>
  402194:       add    rdx,0x1
  402198:       cmp    rdx,0x6
  40219c:       jne    402173 <phase_02+0x83>
  40219e:       jmp    402117 <phase_02+0x27>
```

### 输入数量和栈布局

调用 `valid_u32_fields` 前：

```text
rdi = input
rsi = 6
```

因此本关需要 6 个无符号十进制整数。`sscanf` 的返回值也必须为 6。

六个数从 `[rbp-0x40]` 开始连续存放，每项占 4 字节：

| 数组元素 | 栈地址 |
|---|---|
| `values[0]` | `[rbp-0x40]` |
| `values[1]` | `[rbp-0x3c]` |
| `values[2]` | `[rbp-0x38]` |
| `values[3]` | `[rbp-0x34]` |
| `values[4]` | `[rbp-0x30]` |
| `values[5]` | `[rbp-0x2c]` |

因此：

```asm
mov eax,DWORD PTR [rbp+rdx*4-0x40]
```

等价于读取：

```c
values[rdx]
```

### 首项计算

challenge 保存在 `r13` 中。首项检查对应：

```asm
mov rdx,r13
shr rdx,0x3
and edx,0x7f
add edx,0xa
cmp eax,edx
```

恢复出的关系：

```text
values[0] =
    ((challenge >> 3) & 0x7f) + 10
```

其中 `& 0x7f` 将右移结果截取为低 7 位，所以结果范围是 `10` 到 `137`。

### 递推关系

循环索引从 1 开始：

```asm
mov edx,0x1
```

每轮先计算 challenge 的移位量：

```asm
lea ecx,[rdx*4+0x0]
```

当前 C 实现直接使用变量 `i`，并把 challenge 片段的提取结果命名为
`challenge_bits`：

```c
for (uint32_t i = 1; i <= 5; i++) {
    uint32_t challenge_bits =
        (uint32_t)((challenge >> (4 * i)) & UINT64_C(0x1f));
    values[i] =
        (values[i - 1] + i) * 5 - challenge_bits;
}
```

当 `i = 1, 2, 3, 4, 5` 时，移位量 `4 * i` 分别为
`4, 8, 12, 16, 20`。

每一轮的具体运算为：

```asm
add eax,edx
lea eax,[rax+rax*4]
shr rsi,cl
and esi,0x1f
sub eax,esi
```

含义：

```text
1. 前一个值加上当前索引 i
2. 将中间结果乘以 5
3. 将 challenge 右移 4*i 位
4. 截取移位结果的低 5 位
5. 用前一步的乘积减去截取结果
```

恢复出的递推关系：

```text
对于 i = 1, 2, 3, 4, 5：

values[i] =
    (values[i - 1] + i) * 5 - challenge_bits
```

其中 `challenge_bits` 就是：

```text
(challenge >> (4 * i)) & 0x1f
```

### 循环和最终判定

循环末尾执行：

```asm
add rdx,0x1
cmp rdx,0x6
jne 402173
```

也就是说，索引从 1 递增到 5，共检查后 5 个数。加上单独检查的
`values[0]`，本关总共检查 6 个输入整数。

如果六项全部匹配，程序把此前保存的成功标志放入 `eax` 并返回；任意一项不匹配则
返回失败。

### 恢复出的完整计算流程

```text
读取 6 个无符号整数
→ 计算 values[0]
→ 从 i = 1 开始，使用前一项和 challenge 的 5 位片段递推
→ 直到 i = 5
→ 六项全部匹配才成功
```

### seed 42 验证

challenge：

```text
0x47526757130f9f52
```

求解器生成：

```text
116 564 2799 13985 69930 349659
```

交给炸弹后输出：

```text
Phase 2 defused.
```

### 公开测试

执行：

```bash
python3 public_test.py --phase 2
```

结果：

```text
Phase 2 seed 0: PASS
Phase 2 seed 1: PASS
Phase 2 seed 7: PASS
Phase 2 seed 42: PASS
Phase 2 seed 99: PASS
Phase 2 seed 123: PASS
Phase 2 seed 2026: PASS
Phase 2 seed 65535: PASS
Phase 2 seed 987654321: PASS
Phase 2 seed 18446744073709551615: PASS
Public tests: 10/10
```

### 非公开 seed 验证

使用 seed `2027`：

```bash
./defuser --seed 2027 --phase 2 |
    ./bomb --seed 2027 --phase 2
```

对应的 challenge：

```text
0x39ac678c7345ff51
```

炸弹输出：

```text
Phase 2 defused.
```

### 测试记录

- [x] `make` 编译成功。
- [x] seed 42 单关运行成功。
- [x] `python3 public_test.py --phase 2` 通过，结果为 `10/10`。
- [x] 非公开 seed 2027 验证成功。
- [x] 三项验证截图已保存。

截图说明：

- seed 42 求解器输出截图。
- seed 42 提交给 `bomb` 并显示 `Phase 2 defused.` 的截图。
- 第 2 关公开测试 `10/10` 和/或非公开 seed 2027 的验证截图。

若报告只放一张第 2 关截图，建议使用包含以下内容的终端截图：

```text
Phase 2 challenge
Input (unsigned decimal integers):
Phase 2 defused.
```

## 第 3 关

### 入口关键汇编

```asm
00000000004022d0 <phase_03>:
  4022d0:       endbr64
  4022d4:       push   rbp
  4022d5:       mov    rbp,rsp
  4022d8:       push   r12
  4022da:       mov    r12,rsi
  4022dd:       mov    esi,0x2
  4022e2:       push   rbx
  4022e3:       mov    rbx,rdi
  4022e6:       sub    rsp,0x10
  4022ea:       call   402ca0 <valid_u32_fields>
  4022ef:       test   al,al
  4022f1:       jne    402300 <phase_03+0x30>
  4022f3:       xor    eax,eax
  4022f5:       add    rsp,0x10
  4022f9:       pop    rbx
  4022fa:       pop    r12
  4022fc:       pop    rbp
  4022fd:       ret
  402300:       xor    eax,eax
  402302:       lea    rcx,[rbp-0x14]
  402306:       lea    rdx,[rbp-0x18]
  40230a:       mov    esi,0x403186
  40230f:       mov    rdi,rbx
  402312:       call   4011e0 <__isoc99_sscanf@plt>
  402317:       cmp    eax,0x2
  40231a:       jne    4022f3 <phase_03+0x23>
  40231c:       mov    edi,r12d
  40231f:       xor    eax,eax
  402321:       and    edi,0x7
  402324:       cmp    DWORD PTR [rbp-0x18],edi
  402327:       jne    4022f5 <phase_03+0x25>
  402329:       mov    rax,r12
  40232c:       mov    rsi,r12
  40232f:       shr    rax,0x3
  402333:       shr    rsi,0x20
  402337:       xor    esi,eax
  402339:       call   402270 <p3_dispatch>
  40233e:       cmp    DWORD PTR [rbp-0x14],eax
  402341:       sete   al
  402344:       jmp    4022f5 <phase_03+0x25>
```

### 输入数量和栈布局

调用 `valid_u32_fields` 前：

```text
rdi = input
rsi = 2
```

因此本关需要 2 个无符号十进制整数。

| 输入 | 栈地址 |
|---|---|
| 第一个整数 | `[rbp-0x18]` |
| 第二个整数 | `[rbp-0x14]` |

`sscanf` 必须成功读取 2 项，否则函数返回失败。

### 操作码 `n`

```asm
mov edi,r12d
and edi,0x7
```

恢复出的关系：

```text
n = (uint32_t)challenge & 0x7
```

第一个输入必须等于 `n`：

```asm
cmp DWORD PTR [rbp-0x18],edi
```

所以：

```text
values[0] = challenge & 0x7
```

### 分支参数 `x`

```asm
mov rax,r12
mov rsi,r12
shr rax,0x3
shr rsi,0x20
xor esi,eax
```

恢复出的关系：

```text
x =
    (challenge >> 3) 的低 32 位
    XOR
    challenge 的高 32 位
```

写成 C：

```c
uint32_t x =
    (uint32_t)(challenge >> 3) ^
    (uint32_t)(challenge >> 32);
```

### `p3_dispatch` 和跳转表

```asm
0000000000402270 <p3_dispatch>:
  402274:       mov    eax,edi
  402276:       mov    edi,esi
  402278:       cmp    eax,0x7
  40227b:       ja     4022c2 <p3_dispatch+0x52>
  40227d:       notrack jmp QWORD PTR [rax*8+0x405820]
  402288:       jmp    4021f0 <p3_case_3>
  402290:       jmp    4021b0 <p3_case_0>
  402298:       jmp    402230 <p3_case_5>
  4022a0:       jmp    402210 <p3_case_4>
  4022a8:       jmp    402250 <p3_case_6>
  4022b0:       jmp    4021c0 <p3_case_1>
  4022b8:       jmp    4021d0 <p3_case_2>
  4022c0:       jmp    402260 <p3_case_7>
  4022c2:       xor    eax,eax
  4022c4:       ret
```

调用前：

```text
edi = n
esi = x
```

函数内部交换参数：

```text
eax = n
edi = x
```

然后根据 `n` 查询位于 `0x405820` 的跳转表。

跳转表内容：

| 操作码 | 跳转表项地址 | 表项指向的桩 |
|---|---|---|
| 0 | `0x405820` | `0x402298` |
| 1 | `0x405828` | `0x4022a0` |
| 2 | `0x405830` | `0x4022a8` |
| 3 | `0x405838` | `0x4022b0` |
| 4 | `0x405840` | `0x4022b8` |
| 5 | `0x405848` | `0x4022c0` |
| 6 | `0x405850` | `0x402288` |
| 7 | `0x405858` | `0x402290` |

经过跳转桩后的实际映射：

| 操作码 | 最终调用的 case |
|---|---|
| 0 | `p3_case_5` |
| 1 | `p3_case_4` |
| 2 | `p3_case_6` |
| 3 | `p3_case_1` |
| 4 | `p3_case_2` |
| 5 | `p3_case_7` |
| 6 | `p3_case_3` |
| 7 | `p3_case_0` |

这里不能根据跳转表项的打印顺序或 case 函数名的数字顺序直接猜测映射关系。

### 八个 case 的计算关系

#### `p3_case_0`

```asm
lea eax,[rdi+rdi*4+0x45d0]
```

```text
case0(x) = x * 5 + 0x45d0
```

#### `p3_case_1`

```asm
lea eax,[rdi+rdi*4+0x2c66e]
```

```text
case1(x) = x * 5 + 0x2c66e
```

#### `p3_case_2`

```asm
mov eax,edi
shr edi,0x2
xor eax,0x8d39
add eax,edi
```

```text
case2(x) = (x XOR 0x8d39) + (x >> 2)
```

#### `p3_case_3`

```asm
lea eax,[rdi*4+0x0]
xor eax,0x887c
```

```text
case3(x) = (x * 4) XOR 0x887c
```

#### `p3_case_4`

```asm
lea eax,[rdi*4+0x0]
xor eax,0x732e
```

```text
case4(x) = (x * 4) XOR 0x732e
```

#### `p3_case_5`

```asm
mov eax,edi
shr edi,0x2
xor eax,0x5939
add eax,edi
```

```text
case5(x) = (x XOR 0x5939) + (x >> 2)
```

#### `p3_case_6`

```asm
lea eax,[rdi+rdi*4+0x313d0]
```

```text
case6(x) = x * 5 + 0x313d0
```

#### `p3_case_7`

```asm
lea eax,[rdi+rdi*4+0x1abd]
```

```text
case7(x) = x * 5 + 0x1abd
```

### 按操作码整理

| 操作码 | 计算公式 |
|---|---|
| 0 | `(x ^ 0x5939) + (x >> 2)` |
| 1 | `(x * 4) ^ 0x732e` |
| 2 | `x * 5 + 0x313d0` |
| 3 | `x * 5 + 0x2c66e` |
| 4 | `(x ^ 0x8d39) + (x >> 2)` |
| 5 | `x * 5 + 0x1abd` |
| 6 | `(x * 4) ^ 0x887c` |
| 7 | `x * 5 + 0x45d0` |

### 完整计算流程

```text
n = (uint32_t)challenge & 0x7
x = (uint32_t)(challenge >> 3) ^ (uint32_t)(challenge >> 32)
result = n 在跳转表中对应的 case(x)

values[0] = n
values[1] = result
count = 2
```

当前 C 实现使用同名的 `n`、`x` 和 `result`；最终 `phase_03` 使用第二个输入与
`result` 比较。

### seed 42 验证

challenge：

```text
0x581ce1ff0e4ae394
```

求解器生成：

```text
4 3897204759
```

交给炸弹后输出：

```text
Phase 3 defused.
```

### 公开测试

执行：

```bash
python3 public_test.py --phase 3
```

结果：

```text
Phase 3 seed 0: PASS
Phase 3 seed 1: PASS
Phase 3 seed 7: PASS
Phase 3 seed 42: PASS
Phase 3 seed 99: PASS
Phase 3 seed 123: PASS
Phase 3 seed 2026: PASS
Phase 3 seed 65535: PASS
Phase 3 seed 987654321: PASS
Phase 3 seed 18446744073709551615: PASS
Public tests: 10/10
```

### 非公开 seed 验证

使用 seed `2025`：

```bash
./defuser --seed 2025 --phase 3 |
    ./bomb --seed 2025 --phase 3
```

对应的 challenge：

```text
0x2d942d47fd1ccf80
```

炸弹输出：

```text
Phase 3 defused.
```

### 测试记录

- [x] `make` 编译成功。
- [x] seed 42 单关运行成功。
- [x] `python3 public_test.py --phase 3` 通过，结果为 `10/10`。
- [x] 非公开 seed 2025 验证成功。
- [x] 三项验证截图已保存。

截图说明：

- seed 42 求解器及炸弹验证截图。
- 第 3 关公开测试 `10/10` 截图。
- 非公开 seed 2025 验证截图。

## 第 4 关

### 外层入口关键汇编

```asm
00000000004023c0 <phase_04>:
  4023c0:       endbr64
  4023c4:       push   rbp
  4023c5:       mov    rbp,rsp
  4023c8:       push   r12
  4023ca:       mov    r12,rsi
  4023cd:       mov    esi,0x1
  4023d2:       push   rbx
  4023d3:       mov    rbx,rdi
  4023d6:       sub    rsp,0x10
  4023da:       call   402ca0 <valid_u32_fields>
  4023df:       test   al,al
  4023e1:       jne    4023f0 <phase_04+0x30>
  4023e3:       add    rsp,0x10
  4023e7:       xor    eax,eax
  4023e9:       pop    rbx
  4023ea:       pop    r12
  4023ec:       pop    rbp
  4023ed:       ret
  4023f0:       xor    eax,eax
  4023f2:       lea    rdx,[rbp-0x14]
  4023f6:       mov    esi,0x403189
  4023fb:       mov    rdi,rbx
  4023fe:       call   4011e0 <__isoc99_sscanf@plt>
  402403:       cmp    eax,0x1
  402406:       jne    4023e3 <phase_04+0x23>
  402408:       mov    rcx,r12
  40240b:       mov    rax,r12
  40240e:       mov    edi,r12d
  402411:       xor    esi,esi
  402413:       shr    rax,0x20
  402417:       shr    rcx,0x7
  40241b:       and    edi,0x7f
  40241e:       mov    edx,0x7f
  402423:       xor    ecx,eax
  402425:       call   402350 <p4_walk>
  40242a:       cmp    DWORD PTR [rbp-0x14],eax
  40242d:       sete   al
  402430:       add    rsp,0x10
  402434:       pop    rbx
  402435:       pop    r12
  402437:       pop    rbp
  402438:       ret
```

### 输入与初始参数

调用 `valid_u32_fields` 前：

```text
rdi = input
rsi = 1
```

因此第 4 关需要 1 个无符号十进制整数，用户输入保存在 `[rbp-0x14]`。

调用 `p4_walk` 前的四个参数：

| 参数寄存器 | 含义 | 计算方式 |
|---|---|---|
| `edi` | `target` | `challenge & 0x7f` |
| `esi` | `low` | `0` |
| `edx` | `high` | `0x7f`，即 127 |
| `ecx` | `salt` | `((challenge >> 7) 的低 32 位) XOR challenge 高 32 位` |

恢复为：

```c
uint32_t target = (uint32_t)(challenge & UINT64_C(0x7f));
uint32_t low = UINT32_C(0);
uint32_t high = UINT32_C(0x7f);
uint32_t salt =
    (uint32_t)(challenge >> UINT32_C(7)) ^
    (uint32_t)(challenge >> UINT32_C(32));

values[0] = p4_walk(target, low, high, salt);
count = 1;
```

外层最终比较：

```text
用户输入 == values[0]
```

### `p4_walk` 关键汇编

```asm
0000000000402350 <p4_walk>:
  402350:       endbr64
  402354:       mov    eax,edi
  402356:       mov    edi,edx
  402358:       sub    edi,esi
  40235a:       shr    edi,1
  40235c:       add    edi,esi
  40235e:       cmp    edi,eax
  402360:       je     4023b0 <p4_walk+0x60>
  402362:       push   rbp
  402363:       mov    rbp,rsp
  402366:       push   rbx
  402367:       lea    ebx,[rdi+rcx*1]
  40236a:       sub    rsp,0x8
  40236e:       cmp    eax,edi
  402370:       jb     402390 <p4_walk+0x40>
  402372:       lea    esi,[rdi+0x1]
  402375:       mov    edi,eax
  402377:       call   402350 <p4_walk>
  40237c:       lea    eax,[rax+rax*4]
  40237f:       xor    eax,ebx
  402381:       mov    rbx,QWORD PTR [rbp-0x8]
  402385:       leave
  402386:       xor    eax,0x3
  402389:       ret
  402390:       lea    edx,[rdi-0x1]
  402393:       mov    edi,eax
  402395:       call   402350 <p4_walk>
  40239a:       lea    eax,[rax+rax*4]
  40239d:       xor    eax,ebx
  40239f:       mov    rbx,QWORD PTR [rbp-0x8]
  4023a3:       leave
  4023a4:       xor    eax,0xa
  4023a7:       ret
  4023b0:       lea    eax,[rdi+0xe1]
  4023b6:       xor    eax,ecx
  4023b8:       ret
```

### 中点计算

```asm
mov eax,edi
mov edi,edx
sub edi,esi
shr edi,1
add edi,esi
```

恢复出的关系：

```text
middle = low + (high - low) / 2
```

其中：

```text
eax = target
edi = middle
esi = low
edx = high
ecx = salt
```

### 基准出口

```asm
cmp edi,eax
je 4023b0
```

当：

```text
target == middle
```

进入基准出口：

```asm
lea eax,[rdi+0xe1]
xor eax,ecx
ret
```

恢复出的关系：

```text
return (middle + 0xe1) XOR salt
```

### 左区间递归

```asm
cmp eax,edi
jb 402390
```

当：

```text
target < middle
```

构造：

```text
target = target
low    = low
high   = middle - 1
salt   = salt
```

递归返回后执行：

```asm
lea eax,[rax+rax*4]
xor eax,ebx
xor eax,0xa
```

恢复出的关系：

```text
result = p4_walk(target, low, middle - 1, salt)

return
    (result * 5)
    XOR (middle + salt)
    XOR 0xa
```

### 右区间递归

当：

```text
target > middle
```

构造：

```text
target = target
low    = middle + 1
high   = high
salt   = salt
```

递归返回后执行：

```asm
lea eax,[rax+rax*4]
xor eax,ebx
xor eax,0x3
```

恢复出的关系：

```text
result = p4_walk(target, middle + 1, high, salt)

return
    (result * 5)
    XOR (middle + salt)
    XOR 0x3
```

### 完整递归关系

```text
p4_walk(target, low, high, salt):

middle = low + (high - low) / 2

如果 target == middle：
    return (middle + 0xe1) XOR salt

如果 target < middle：
    result = p4_walk(target, low, middle - 1, salt)
    return (result * 5) XOR (middle + salt) XOR 0xa

如果 target > middle：
    result = p4_walk(target, middle + 1, high, salt)
    return (result * 5) XOR (middle + salt) XOR 0x3
```

所有中间值均按 32 位无符号整数处理。`middle + salt` 是加法，不是异或。

### GDB 验证

seed 42 对应的 challenge：

```text
0x09bc585a244823f2
```

第一次进入 `p4_walk` 时：

```text
target = 0x72 = 114
low    = 0
high   = 127
salt   = 0xbdf4c81d
```

第一次计算：

```text
middle = 0 + (127 - 0) / 2 = 63
```

由于 `114 > 63`，进入右半区间。

递归最终到达基准情形：

```text
target = 114
middle = 114
```

基准结果：

```text
(0x72 + 0xe1) XOR 0xbdf4c81d
= 0x153 XOR 0xbdf4c81d
= 0xbdf4c94e
```

GDB 在基准出口执行完毕后观察到：

```text
p/x $eax = 0xbdf4c94e
```

与手算结果一致。该值回到上一层后还会继续执行乘 5 和两次异或，因此基准结果不是第 4 关最终答案。

### seed 42 验证

执行：

```bash
./defuser --seed 42 --phase 4 |
    ./bomb --seed 42 --phase 4
```

炸弹输出：

```text
Phase 4 defused.
```

### 公开测试

执行：

```bash
python3 public_test.py --phase 4
```

结果：

```text
Phase 4 seed 0: PASS
Phase 4 seed 1: PASS
Phase 4 seed 7: PASS
Phase 4 seed 42: PASS
Phase 4 seed 99: PASS
Phase 4 seed 123: PASS
Phase 4 seed 2026: PASS
Phase 4 seed 65535: PASS
Phase 4 seed 987654321: PASS
Phase 4 seed 18446744073709551615: PASS
Public tests: 10/10
```

### 非公开 seed 验证

使用 seed `2027`：

```bash
./defuser --seed 2027 --phase 4 |
    ./bomb --seed 2027 --phase 4
```

对应的 challenge：

```text
0xba45ec0b3d1d960d
```

炸弹输出：

```text
Phase 4 defused.
```

### 测试记录

- [x] `make` 编译成功。
- [x] seed 42 单关运行成功。
- [x] `python3 public_test.py --phase 4` 通过，结果为 `10/10`。
- [x] 非公开 seed 2027 验证成功。
- [x] 第 4 关运行截图已保存（具体文件名待补充）。

## 第 5 关

### 外层 `phase_05` 关键汇编

```asm
00000000004024a0 <phase_05>:
  4024a0:       endbr64
  4024a4:       push   rbp
  4024a5:       mov    rbp,rsp
  4024a8:       push   r12
  4024aa:       mov    r12,rsi
  4024ad:       mov    esi,0x2
  4024b2:       push   rbx
  4024b3:       mov    rbx,rdi
  4024b6:       sub    rsp,0x10
  4024ba:       call   402ca0 <valid_u32_fields>
  4024bf:       test   al,al
  4024c1:       jne    4024d0 <phase_05+0x30>
  4024c3:       xor    eax,eax
  4024c5:       add    rsp,0x10
  4024c9:       pop    rbx
  4024ca:       pop    r12
  4024cc:       pop    rbp
  4024cd:       ret
  4024d0:       xor    eax,eax
  4024d2:       lea    rcx,[rbp-0x18]
  4024d6:       lea    rdx,[rbp-0x1c]
  4024da:       mov    esi,0x403186
  4024df:       mov    rdi,rbx
  4024e2:       call   4011e0 <__isoc99_sscanf@plt>
  4024e7:       cmp    eax,0x2
  4024ea:       jne    4024c3 <phase_05+0x23>
  4024ec:       lea    rsi,[rbp-0x14]
  4024f0:       mov    rdi,r12
  4024f3:       call   402440 <p5_traverse>
  4024f8:       mov    edx,eax
  4024fa:       xor    eax,eax
  4024fc:       cmp    DWORD PTR [rbp-0x1c],edx
  4024ff:       jne    4024c5 <phase_05+0x25>
  402501:       mov    eax,DWORD PTR [rbp-0x14]
  402504:       cmp    DWORD PTR [rbp-0x18],eax
  402507:       sete   al
  40250a:       jmp    4024c5 <phase_05+0x25>
```

### 输入和比较关系

本关需要 2 个无符号十进制整数：

| 输入 | 栈地址 | 含义 |
|---|---|---|
| 第一个输入 | `[rbp-0x1c]` | 预期的遍历累计值 |
| 第二个输入 | `[rbp-0x18]` | 预期的最后节点 tag |

helper 通过参数 `computed_tag` 写回标签的位置：

```text
[rbp-0x14]
```

调用关系：

```c
uint32_t actual_tag = 0;
uint32_t actual_accumulator =
    p5_traverse(challenge, &actual_tag);
```

两个检查条件：

```text
第一个输入 == actual_accumulator
第二个输入 == actual_tag
```

求解器输出顺序：

```text
values[0] = 累计值
values[1] = 最后访问节点的 tag
count = 2
```

### `p5_traverse` 关键汇编

```asm
0000000000402440 <p5_traverse>:
  402440:       endbr64
  402444:       mov    rax,rdi
  402447:       mov    rcx,rdi
  40244a:       mov    r8,rsi
  40244d:       and    eax,0x7
  402450:       shr    rcx,0x3
  402454:       mov    rdx,QWORD PTR [rax*8+0x405b60]
  40245c:       mov    rax,rdi
  40245f:       shr    rdi,0x20
  402463:       shr    rax,0x6
  402467:       xor    eax,edi
  402469:       mov    esi,DWORD PTR [rdx+0x4]
  40246c:       mov    edi,ecx
  40246e:       xor    eax,0xa582
  402473:       lea    eax,[rax+rax*2]
  402476:       add    eax,DWORD PTR [rdx]
  402478:       mov    DWORD PTR [r8],esi
  40247b:       xor    eax,esi
  40247d:       and    edi,0x3
  402480:       je     40249f <p5_traverse+0x5f>
  402482:       mov    ecx,0x1
  402487:       mov    rdx,QWORD PTR [rdx+0x8]
  40248b:       lea    eax,[rax+rax*2]
  40248e:       add    ecx,0x1
  402491:       mov    esi,DWORD PTR [rdx+0x4]
  402494:       add    eax,DWORD PTR [rdx]
  402496:       xor    eax,esi
  402498:       mov    DWORD PTR [r8],esi
  40249b:       cmp    edi,ecx
  40249d:       jae    402487 <p5_traverse+0x47>
  40249f:       ret
```

### 原始内存布局与当前 C 表示

代码中的三类内存访问：

```asm
DWORD PTR [rdx]
DWORD PTR [rdx+0x4]
QWORD PTR [rdx+0x8]
```

反汇编对应的原始内存结构为：

```c
struct Node {
    uint32_t value;          // offset 0
    uint32_t tag;            // offset 4
    const struct Node *next; // offset 8
};
```

当前 `defuser.c` 改用数组索引表示 `next`，以便避免直接复制虚拟地址。字段名与
节点顺序保持一致：

```c
struct Node {
    uint32_t value;
    uint32_t tag;
    int next_index;          // -1 表示 NULL
};
```

当前 C 中的实际局部变量类型为：

```c
uint32_t head_index;
int node_index;
struct Node node;
uint32_t accumulator;
uint32_t extra_steps;
uint32_t i;
```

### 链表头数据

原始 8 个链表头位于 `0x405b60`：

| `head_index` | 原始地址 | 数组索引 |
|---|---:|---:|
| 0 | `0x405bb0` | 1 |
| 1 | `0x405c50` | 11 |
| 2 | `0x405bd0` | 3 |
| 3 | `0x405c20` | 8 |
| 4 | `0x405bc0` | 2 |
| 5 | `0x405bf0` | 5 |
| 6 | `0x405c00` | 6 |
| 7 | `0x405c10` | 7 |

索引形式：

```text
heads = { 1, 11, 3, 8, 2, 5, 6, 7 }
```

### 完整节点表

| 数组索引 | 原始地址 | value | tag | next 索引 |
|---:|---|---:|---:|---:|
| 0 | `0x405ba0` | 60077 | 285 | 10 |
| 1 | `0x405bb0` | 22543 | 1544 | 11 |
| 2 | `0x405bc0` | 47206 | 1888 | 5 |
| 3 | `0x405bd0` | 13497 | 2632 | 8 |
| 4 | `0x405be0` | 4958 | 895 | `-1` |
| 5 | `0x405bf0` | 18360 | 31 | 6 |
| 6 | `0x405c00` | 51270 | 2914 | 7 |
| 7 | `0x405c10` | 7752 | 564 | 0 |
| 8 | `0x405c20` | 23209 | 1335 | 2 |
| 9 | `0x405c30` | 13448 | 1125 | 4 |
| 10 | `0x405c40` | 51348 | 2430 | 9 |
| 11 | `0x405c50` | 29473 | 2091 | 3 |

### 展开后的链表

```text
head 0: 1 → 11 → 3 → 8 → 2 → 5 → 6 → 7 → 0 → 10 → 9 → 4 → NULL

head 1: 11 → 3 → 8 → 2 → 5 → 6 → 7 → 0 → 10 → 9 → 4 → NULL

head 2: 3 → 8 → 2 → 5 → 6 → 7 → 0 → 10 → 9 → 4 → NULL

head 3: 8 → 2 → 5 → 6 → 7 → 0 → 10 → 9 → 4 → NULL

head 4: 2 → 5 → 6 → 7 → 0 → 10 → 9 → 4 → NULL

head 5: 5 → 6 → 7 → 0 → 10 → 9 → 4 → NULL

head 6: 6 → 7 → 0 → 10 → 9 → 4 → NULL

head 7: 7 → 0 → 10 → 9 → 4 → NULL
```

这些链表没有回路，最终都终止于节点索引 4。

### `p5_traverse` 数据流

#### 选择链表头和额外步数

```text
head_index = challenge & 0x7
node_index = heads[head_index]

extra_steps = (challenge >> 3) & 0x3
```

完整 challenge 右移 3 位后再截取低 2 位。

当前 C 的对应声明为：

```c
uint32_t head_index =
    (uint32_t)(challenge & UINT32_C(0x7));
int node_index = heads[head_index];
uint32_t extra_steps =
    (uint32_t)((challenge >> 3) & UINT64_C(0x3));
```

#### 初始混合值

```text
mixed =
    (challenge >> 6) 的低 32 位
    XOR
    challenge 的高 32 位
```

当前 C 中分别使用 `low_part`、`high_part` 和 `mixed`：

```c
uint32_t low_part = (uint32_t)(challenge >> 6);
uint32_t high_part = (uint32_t)(challenge >> 32);
uint32_t mixed = low_part ^ high_part;
```

#### 第一个节点

```text
*computed_tag = node.tag

accumulator =
    (
        (mixed XOR 0xa582) * 3
        + node.value
    ) XOR node.tag
```

第一个节点一定会访问。

#### 后续节点

对于每个额外节点，当前 C 先更新 `node_index`，再从 `nodes` 取出新的 `node`：

```text
node_index = nodes[node_index].next_index
node = nodes[node_index]

accumulator =
    accumulator * 3 + node.value

*computed_tag = node.tag
accumulator =
    accumulator XOR node.tag
```

最终返回 `accumulator`，并通过 `*computed_tag` 把最后访问节点的 tag 写回调用者。

### 完整伪代码

```text
p5_traverse(challenge, computed_tag):

    head_index = challenge & 0x7
    node_index = heads[head_index]

    mixed =
        ((challenge >> 6) 的低 32 位)
        XOR challenge 高 32 位

    node = nodes[node_index]

    accumulator =
        ((mixed XOR 0xa582) * 3 + node.value)
        XOR node.tag

    *computed_tag = node.tag

    extra_steps = (challenge >> 3) & 0x3

    对 i = 0 到 extra_steps - 1：
        node_index = nodes[node_index].next_index
        node = nodes[node_index]
        accumulator = accumulator * 3 + node.value
        *computed_tag = node.tag
        accumulator = accumulator XOR node.tag

    return accumulator
```

### seed 42 验证

challenge：

```text
0xde4431fa3c80db06
```

链表头索引：

```text
challenge & 7 = 6
heads[6] = nodes[6]
```

节点索引 6：

```text
value = 51270 = 0xc846
tag   = 2914  = 0xb62
next  = nodes[7]
```

完整 challenge 右移 3 位：

```text
0xde4431fa3c80db06 >> 3
= 0x1bc8863e47901b60
```

低 32 位为 `0x47901b60`，额外步数：

```text
0x47901b60 & 3 = 0
```

因此 seed 42 只访问 `nodes[6]`。

初始混合值：

```text
(challenge >> 6 的低 32 位) XOR challenge 高 32 位
= 0xe8f2036c XOR 0xde4431fa
= 0x36b63296
```

累计值：

```text
0x36b63296 XOR 0xa582
= 0x36b69714

0x36b69714 * 3
= 0xa423c53c

0xa423c53c + 0xc846
= 0xa4248d82

0xa4248d82 XOR 0xb62
= 0xa42486e0
= 2753857248
```

GDB 在 `0x402480` 观察到：

```text
edi = 0
eax = 0xa42486e0
```

因此 seed 42 的求解器输出为：

```text
2753857248 2914
```

交给炸弹后输出：

```text
Phase 5 defused.
```

### 公开测试

执行：

```bash
python3 public_test.py --phase 5
```

结果：

```text
Phase 5 seed 0: PASS
Phase 5 seed 1: PASS
Phase 5 seed 7: PASS
Phase 5 seed 42: PASS
Phase 5 seed 99: PASS
Phase 5 seed 123: PASS
Phase 5 seed 2026: PASS
Phase 5 seed 65535: PASS
Phase 5 seed 987654321: PASS
Phase 5 seed 18446744073709551615: PASS
Public tests: 10/10
```

### 非公开 seed 验证

使用 seed `2025`：

```bash
./defuser --seed 2025 --phase 5 |
    ./bomb --seed 2025 --phase 5
```

对应的 challenge：

```text
0xab9627bdce21e915
```

炸弹输出：

```text
Phase 5 defused.
```

### 测试记录

- [x] `make` 编译成功。
- [x] seed 42 单关运行成功。
- [x] `python3 public_test.py --phase 5` 通过，结果为 `10/10`。
- [x] 非公开 seed 2025 验证成功。
- [x] 第 5 关运行截图已保存（具体文件名待补充）。

## 第 6 关

### 外层 `phase_06` 关键汇编

```asm
00000000004025f0 <phase_06>:
  4025f0:       endbr64
  4025f4:       push   rbp
  4025f5:       mov    rbp,rsp
  4025f8:       push   r12
  4025fa:       mov    r12,rsi
  4025fd:       mov    esi,0x1
  402602:       push   rbx
  402603:       mov    rbx,rdi
  402606:       sub    rsp,0x10
  40260a:       call   402ca0 <valid_u32_fields>
  40260f:       test   al,al
  402611:       jne    402620 <phase_06+0x30>
  402613:       add    rsp,0x10
  402617:       xor    eax,eax
  402619:       pop    rbx
  40261a:       pop    r12
  40261c:       pop    rbp
  40261d:       ret
  402620:       xor    eax,eax
  402622:       lea    rdx,[rbp-0x14]
  402626:       mov    esi,0x403189
  40262b:       mov    rdi,rbx
  40262e:       call   4011e0 <__isoc99_sscanf@plt>
  402633:       cmp    eax,0x1
  402636:       jne    402613 <phase_06+0x23>
  402638:       mov    rdi,r12
  40263b:       call   4025a0 <p6_dispatch>
  402640:       cmp    DWORD PTR [rbp-0x14],eax
  402643:       sete   al
  402646:       add    rsp,0x10
  40264a:       pop    rbx
  40264b:       pop    r12
  40264d:       pop    rbp
  40264e:       ret
```

### 外层输入和检查关系

第 6 关需要 1 个无符号十进制整数，用户输入保存在 `[rbp-0x14]`。

当前 C 直接保存 helper 返回值：

```c
values[0] = p6_dispatch(challenge);
count = 1;
```

最终检查：

```text
用户输入 == p6_dispatch(challenge)
```

求解器输出：

```text
values[0] = p6_dispatch(challenge)
count = 1
```

### `p6_dispatch` 关键汇编

```asm
00000000004025a0 <p6_dispatch>:
  4025a0:       endbr64
  4025a4:       push   rbp
  4025a5:       mov    rbp,rsp
  4025a8:       push   rbx
  4025a9:       mov    rbx,rdi
  4025ac:       shr    rdi,0x6
  4025b0:       mov    rax,rbx
  4025b3:       shr    rax,0x20
  4025b7:       xor    edi,eax
  4025b9:       mov    rax,rbx
  4025bc:       sub    rsp,0x8
  4025c0:       and    ebx,0x38
  4025c3:       and    eax,0x7
  4025c6:       call   QWORD PTR [rax*8+0x405e00]
  4025cd:       xor    eax,0xc31b
  4025d2:       mov    edi,eax
  4025d4:       call   QWORD PTR [rbx+0x405e00]
  4025da:       mov    rbx,QWORD PTR [rbp-0x8]
  4025de:       leave
  4025df:       add    eax,0xb338
  4025e4:       ret
```

### 载荷和两个函数表索引

函数参数：

```text
payload =
    (challenge >> 6) 的低 32 位
    XOR
    challenge 的高 32 位
```

第一次函数索引：

```text
first_index = challenge & 0x7
```

第二次函数索引：

```text
second_index = (challenge >> 3) & 0x7
```

`challenge & 0x38` 实际保存的是第二个索引乘 8 后的字节偏移：

```text
0x38 = 7 << 3
```

### 函数指针表

函数指针表位于 `0x405e00`：

| 表索引 | 原始指针 | 实际函数 |
|---:|---|---|
| 0 | `0x402530` | `p6_op_1` |
| 1 | `0x402540` | `p6_op_2` |
| 2 | `0x402530` | `p6_op_1` |
| 3 | `0x402580` | `p6_op_5` |
| 4 | `0x402560` | `p6_op_4` |
| 5 | `0x402550` | `p6_op_3` |
| 6 | `0x402560` | `p6_op_4` |
| 7 | `0x402510` | `p6_op_0` |

对应关系：

```text
operations[0] = p6_op_1
operations[1] = p6_op_2
operations[2] = p6_op_1
operations[3] = p6_op_5
operations[4] = p6_op_4
operations[5] = p6_op_3
operations[6] = p6_op_4
operations[7] = p6_op_0
```

函数表索引和函数名末尾编号并不相同。

当前 C 保存函数指针表时使用：

```c
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
```

各操作函数均使用参数名 `x`。

### 六个操作函数

#### `p6_op_0`

```asm
mov eax,edi
lea edx,[rdi+rdi*2]
shl eax,0x7
add eax,0x6d54
xor eax,edx
```

```text
p6_op_0(x) =
    ((x << 7) + 0x6d54)
    XOR
    (x * 3)
```

#### `p6_op_1`

```asm
mov eax,edi
rol eax,1
xor eax,0xe6cc
```

```text
p6_op_1(x) =
    rotate_left_1(x) XOR 0xe6cc
```

32 位循环左移：

```text
(x << 1) | (x >> 31)
```

#### `p6_op_2`

```asm
mov eax,edi
ror eax,0x7
```

```text
p6_op_2(x) =
    rotate_right_7(x)
```

32 位循环右移：

```text
(x >> 7) | (x << 25)
```

`rotl1` 和 `rotr7` 只是本文用于表示循环移位的数学名称；当前 C 中没有同名的
函数，实际分别在 `p6_op_1` 和 `p6_op_2` 内用上面的移位、按位或表达式实现。

#### `p6_op_3`

```asm
lea eax,[rdi+0x499d]
shr edi,0x5
xor eax,edi
```

```text
p6_op_3(x) =
    (x + 0x499d) XOR (x >> 5)
```

#### `p6_op_4`

```asm
mov eax,edi
shl edi,0x7
xor eax,0xb913
add eax,edi
```

```text
p6_op_4(x) =
    (x XOR 0xb913) + (x << 7)
```

#### `p6_op_5`

```asm
mov eax,edi
shr eax,0x2
xor eax,edi
add eax,0xbda6
```

```text
p6_op_5(x) =
    ((x >> 2) XOR x) + 0xbda6
```

### 按函数表索引整理

| 表索引 | 实际函数 | 计算公式 |
|---:|---|---|
| 0 | `p6_op_1` | `rotl1(x) ^ 0xe6cc` |
| 1 | `p6_op_2` | `rotr7(x)` |
| 2 | `p6_op_1` | `rotl1(x) ^ 0xe6cc` |
| 3 | `p6_op_5` | `((x >> 2) ^ x) + 0xbda6` |
| 4 | `p6_op_4` | `(x ^ 0xb913) + (x << 7)` |
| 5 | `p6_op_3` | `(x + 0x499d) ^ (x >> 5)` |
| 6 | `p6_op_4` | `(x ^ 0xb913) + (x << 7)` |
| 7 | `p6_op_0` | `((x << 7) + 0x6d54) ^ (x * 3)` |

### 完整数据流

```text
payload =
    (challenge >> 6 的低 32 位)
    XOR challenge 高 32 位

first_index  = challenge & 7
second_index = (challenge >> 3) & 7

first_result =
    operations[first_index](payload)

first_result =
    first_result XOR 0xc31b

second_result =
    operations[second_index](first_result)

return second_result + 0xb338
```

所有操作数按 32 位无符号整数处理。

### seed 42 验证

challenge：

```text
0x37e9671c45376d5d
```

最低 3 位：

```text
challenge & 7 = 5
```

第一次选择：

```text
operations[5] = p6_op_3
```

第二组索引：

```text
(challenge >> 3) & 7 = 3
```

第二次选择：

```text
operations[3] = p6_op_5
```

调用链：

```text
p6_op_3
→ XOR 0xc31b
→ p6_op_5
→ 加 0xb338
```

GDB 观察到的中间值：

```text
payload                     = 0x46fdbaa9
p6_op_3 原始返回            = 0x44c9e993
XOR 0xc31b 后               = 0x44c92a88
p6_op_5 返回后、加常量之前  = 0x55fc1dd0
最终结果                    = 0x55fcd108
```

最终值转换为无符号十进制：

```text
0x55fcd108 = 1442631944
```

求解器输出：

```text
1442631944
```

交给炸弹后输出：

```text
Phase 6 defused.
```

### 公开测试

执行：

```bash
python3 public_test.py --phase 6
```

结果：

```text
Phase 6 seed 0: PASS
Phase 6 seed 1: PASS
Phase 6 seed 7: PASS
Phase 6 seed 42: PASS
Phase 6 seed 99: PASS
Phase 6 seed 123: PASS
Phase 6 seed 2026: PASS
Phase 6 seed 65535: PASS
Phase 6 seed 987654321: PASS
Phase 6 seed 18446744073709551615: PASS
Public tests: 10/10
```

### 非公开 seed 验证

使用 seed `2027`：

```bash
./defuser --seed 2027 --phase 6 |
    ./bomb --seed 2027 --phase 6
```

对应的 challenge：

```text
0xb61ba54a21ee5bf5
```

炸弹输出：

```text
Phase 6 defused.
```

### 测试记录

- [x] `make` 编译成功。
- [x] seed 42 单关运行成功。
- [x] `python3 public_test.py --phase 6` 通过，结果为 `10/10`。
- [x] 非公开 seed 2027 验证成功。
- [x] 第 6 关运行截图已保存（具体文件名待补充）。

