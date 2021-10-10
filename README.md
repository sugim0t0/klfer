# KLFER (ver 0.4)

## 概要

KLFER(Kernel Logger for Function Entries and Returns)はカーネル空間のシンボル関数について入場(Entry)と退場(Return)時にログを吐く機能を持ちます。 
それぞれのタイミングでタイムスタンプを押すこともできます。 
ログを吐くLKM(Loadable Kernel Module)と関数の登録等を制御するアプリケーションで構成されます。 
ツリー構成は以下の様になります。
```
.
|-- README.md        # 本ファイル
|-- app
|   |-- Makefile     # アプリケーション用Makefile
|   `-- klfer_app.c  # アプリケーションソースコード
|-- build.sh         # Build/Cleanスクリプト
|-- include
|   `-- klfer_api.h  # LKMのアプリケーション向け公開APIヘッダファイル
`-- mod
    |-- Makefile     # LKM用Makefile
    |-- klfer.h      # LKMヘッダファイル
    |-- klfer_dbg.c  # LKMデバッグモードソースコード
    |-- klfer_dbg.h  # LKMデバッグモードヘッダファイル
    `-- klfer_mod.c  # LKMソースコード
```
また、KLFERを使用するにはLinux KernelのKPROBES, KRETPROBES configurationsがそれぞれ有効(=y)になっている必要があります。
```
$ cat /boot/config-$(uname -r) | grep -e KPROBES -e KRETPROBES
$ zcat /proc/config.gz | grep -e KPROBES -e KRETPROBES
```
上記等のコマンドでconfigurationsを確認できます。

## Build / Clean

Build/Cleanスクリプト(build.sh)を使用してLKM及びアプリケーションをまとめてBuild/Cleanすることができます。

### Build

```
$ ./build.sh
```
Buildで生成したバイナリファイル(klfer.ko, klferctl)はbinディレクトリ下に置かれます。

### Clean

```
$ ./build.sh clean
```

## 使用方法

DebugモードでBuildすると、サンプル関数(klfer_sample_func, klfer_sample_nested_func)で使用方法を確認できます。 
以下に、使用方法として手順を記載します。

### LKMインストール

```
$ cd bin/
$ insmod klfer.ko
```

### klferctl(アプリケーション)使い方

usageを```-h```オプションで出力できます。

```
$ ./klferctl -h
Usage:
  klferctl {-A <FUNC>|-D <FUNC>|-R|{[-E|-d] [-J|-j] [-T<FMT>|-t]}|-S|-L|-h}

    -A <FUNC>     Add new function(<FUNC>(*1)) to be registered
    -D <FUNC>     Delete registered function(<FUNC>(*1))
    -R            Reset (delete all registered functions and logs)
    -E | -d       Enable logger(-E) / Disable logger(-d) (default: Disable)
    -J | -j       Enable JIT print log(*3)(-J) / Disable JIT print log(-j) (default: Disable)
    -T<FMT> | -t  Enable Timestamp(-T<FMT>(*2)) / Disable Timestamp(-t) (default: Enable)
    -S            Dump current settings and registered functions
    -L            Dump Logs
    -h            Help

  SAMPLE COMMAND:
    -s            Call sample function (klfer_sample_func)
    ex) $ klferctl -s

  (*1) <FUNC>       : Function name to be logged. MUST be symbol in kernel
  (*2) <FMT> {0..2} : Timestamp output format
     # -T0 > Absolute time (default)
     # -T1 > Relative time from the first log
     # -T2 > Relative time from the previous log
  (*3) JIT(Just-In-Time) print log
     # Print a log each time.
     # So, timestamp contains printk processing time.
     # Just-In-Time print log should not be used
     #   if you want to measure function processing time.
```

まずサンプル関数を登録します。

```
$ ./klferctl -A klfer_sample_func
$ ./klferctl -A klfer_sample_nested_func
```

登録した関数や現在の設定値は```-S```オプションで確認できます。

```
$ ./klferctl -S
$ dmesg -t
Logger        : Disable
JIT print log : Disable
Timestamp     : Enable
Timestamp fmt : Absolute time
[Indx] [Reg] function_name
[   0] [ Y ] klfer_sample_func
[   1] [ Y ] klfer_sample_nested_func
```

関数登録後、ログを有効化します。(```-E```オプション)

```
$ ./klferctl -E
```

DebugモードでBuildすると、テストコマンド```-s```でサンプル関数を実行できます。 
サンプル関数は以下のような関数です。

```c
int klfer_sample_func(void)
{
    int i;
    pr_debug("enter %s()\n", __func__);
    for(i=0; i<5; i++)
    {
        klfer_sample_nested_func();
    }
    return KLFER_OK;
}
EXPORT_SYMBOL(klfer_sample_func);

void klfer_sample_nested_func(void)
{
    pr_debug("enter %s()\n", __func__);
}
EXPORT_SYMBOL(klfer_sample_nested_func);
```

登録した関数を実行させます。

```
$ ./klferctl -s
```

ログを確認します。(```-L```オプション)

```
$ ./klferctl -L
$ dmesg -t
[  1629399681954816044 nsec] [1] e klfer_sample_func
[  1629399681954817612 nsec] [2] e klfer_sample_nested_func
[  1629399681954818519 nsec] [3] r klfer_sample_nested_func
[  1629399681954818786 nsec] [4] e klfer_sample_nested_func
[  1629399681954819265 nsec] [5] r klfer_sample_nested_func
[  1629399681954819420 nsec] [6] e klfer_sample_nested_func
[  1629399681954819879 nsec] [7] r klfer_sample_nested_func
[  1629399681954820032 nsec] [8] e klfer_sample_nested_func
[  1629399681954820487 nsec] [9] r klfer_sample_nested_func
[  1629399681954820652 nsec] [10] e klfer_sample_nested_func
[  1629399681954821099 nsec] [11] r klfer_sample_nested_func
[  1629399681954821253 nsec] [12] r klfer_sample_func
```

ログのフォーマットは、
```
[ <TIMESTAMP> nsec] [Sequence No.] <e(Entry)|r(Return)> function_name
```
となります。

ログを無効化します。(```-d```オプション)

```
$ ./klferctl -d
```

### Just-In-Timeログモード
Just-In-Timeログモードでは```-L```オプションで後からまとめてログを出力するのではなく登録した関数が実行されるタイミングでログを出力するモードです。 
このモードではログを出力するprintkや文字列コピー等の処理時間がログのタイムスタンプに含まれるため、関数の処理時間や複数の関数間の時間等を計測するという目的には適しませんが、登録した関数内で出力するログ等との前後関係が分かるというメリットがあります。 
例えば、上記サンプル関数をJust-In-Timeログモードで実行してみます。(```-J```オプション)

```
$ ./klferctl -J -E
$ ./klferctl -s
$ dmesg -t
[  1629400446130525404 nsec] [13] e klfer_sample_func
enter klfer_sample_func()
[  1629400446130527841 nsec] [14] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[  1629400446130529416 nsec] [15] r klfer_sample_nested_func
[  1629400446130530230 nsec] [16] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[  1629400446130531211 nsec] [17] r klfer_sample_nested_func
[  1629400446130531915 nsec] [18] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[  1629400446130532870 nsec] [19] r klfer_sample_nested_func
[  1629400446130533527 nsec] [20] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[  1629400446130534475 nsec] [21] r klfer_sample_nested_func
[  1629400446130535145 nsec] [22] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[  1629400446130536093 nsec] [23] r klfer_sample_nested_func
[  1629400446130536714 nsec] [24] r klfer_sample_func
```
先ほどの```-L```オプションで表示した場合と異なり、サンプル関数内で実行されるprintk(pr_debug)による出力が間に出力されています。

### LKMアンインストール

```
$ rmmod klfer
```
