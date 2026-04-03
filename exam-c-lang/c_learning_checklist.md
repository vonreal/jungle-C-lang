# C언어 학습 체크리스트

> **목표**: Python으로 풀어본 word count를 C로 옮길 수 있을 만큼만 배우기
>
> **소요 시간**: 2~3시간
>
> **학습 방법**: 각 항목의 링크를 읽고, 예제를 직접 쳐보고, 체크하세요.

---

## 1단계: C 프로그램의 뼈대

Python에는 없지만 C에서는 반드시 필요한 것들

- [ ] **Get Started / Syntax** — https://www.w3schools.com/C/c_getstarted.php
  - `#include <stdio.h>`, `main()`, `{}`, `;` 이해하기
  - Python과 달리 진입점(`main`)이 필요하다는 것 확인

- [ ] **Output (Print)** — https://www.w3schools.com/C/c_output.php
  - `printf()` 사용법 확인
  - `\n`을 직접 넣어야 줄바꿈된다는 것 확인

- [ ] **Comments** — https://www.w3schools.com/C/c_comments.php
  - `//`와 `/* */` 확인 (가볍게 보고 넘어가기)

> ✅ **체크포인트**: `Hello, World!`를 출력하는 프로그램을 직접 컴파일하고 실행할 수 있다.

---

## 2단계: 변수와 타입

Python: `x = 10` → C: `int x = 10;`

- [ ] **Variables** — https://www.w3schools.com/C/c_variables.php
  - 타입을 먼저 선언해야 한다는 것 이해
  - `int`, `char` 타입 확인

- [ ] **Data Types** — https://www.w3schools.com/C/c_data_types.php
  - word count에서 쓰는 타입: `int` (카운터), `int` (문자 저장)
  - `char`와 `int`의 관계 (문자는 정수로 저장된다)

- [ ] **Constants** — https://www.w3schools.com/C/c_constants.php
  - `#define IN 1` 같은 매크로 상수 이해
  - Python의 `IN = 1`과 차이 확인

> ✅ **체크포인트**: `int nl = 0;`처럼 변수를 선언하고 값을 대입할 수 있다.

---

## 3단계: 연산자

Python과 거의 같지만, `++`와 `||`가 다르다

- [ ] **Operators** — https://www.w3schools.com/C/c_operators.php
  - `++`(증감 연산자) — Python에 없음!
  - `==`(비교), `||`(or), `&&`(and) 확인
  - `=`(대입)과 `==`(비교) 구분

> ✅ **체크포인트**: `++nc`가 `nc += 1`과 같다는 것을 안다. `||`가 Python의 `or`라는 것을 안다.

---

## 4단계: 조건문

Python: `if/elif/else` → C: `if/else if/else`

- [ ] **If...Else** — https://www.w3schools.com/C/c_conditions.php
  - `if`, `else if`, `else` 문법 확인
  - `()`로 조건을 감싸야 한다는 것 확인
  - Python의 `elif`가 C에서는 `else if`라는 것 확인

> ✅ **체크포인트**: `if (c == '\n')` 같은 조건문을 쓸 수 있다.

---

## 5단계: 반복문

Python: `for c in text:` → C: `while ((c = getchar()) != EOF)`

- [ ] **While Loop** — https://www.w3schools.com/C/c_while_loop.php
  - `while` 문법 확인
  - 조건에 `()`가 필요하다는 것 확인

> ✅ **체크포인트**: `while` 루프를 작성할 수 있다.

---

## 6단계: 입출력

Python: `input()` / `print()` → C: `getchar()` / `printf()`

- [ ] **User Input** — https://www.w3schools.com/C/c_user_input.php
  - `scanf` 존재는 알되, word count에서는 `getchar()` 사용
  - `getchar()`는 한 글자씩 읽고, `EOF`를 만나면 끝

- [ ] **Format Specifiers** — https://www.w3schools.com/C/c_format_specifiers.php
  - `%d` — 정수 출력
  - `printf("%d %d %d\n", nl, nw, nc);` 패턴 확인

> ✅ **체크포인트**: `printf`로 변수 여러 개를 포맷에 맞춰 출력할 수 있다.

---

## 최종 도전: Word Count를 C로 옮기기

위 체크포인트를 모두 통과했으면, Python 코드를 옆에 두고 C로 옮겨보세요.

```
필요한 조각 매핑:
Python                    →  C
─────────────────────────────────────
IN = 1                    →  #define IN 1
state = OUT               →  int state; state = OUT;
for c in text:            →  while ((c = getchar()) != EOF)
nc += 1                   →  ++nc;
if c == '\n':             →  if (c == '\n')
c in (' ', '\n', '\t')   →  c == ' ' || c == '\n' || c == '\t'
elif not state:           →  else if (state == OUT)
print(f"...")             →  printf("%d %d %d\n", nl, nw, nc);
```

### 컴파일 & 실행 방법

```bash
gcc -o wc wc.c
echo "hello world" | ./wc
```

예상 출력: `1 2 12`
