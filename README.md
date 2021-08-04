# KLFER

## 概要

KLFER(Kernel Logger for Function Entries and Returns)はカーネル空間のシンボル関数について入場(Entry)と退場(Return)時にログを吐く機能を持ちます。 
それぞれのタイミングでタイムスタンプを押すこともできます。 
また、複数の関数をグループとして登録し、グループ内での関数実行順としてシーケンス番号をログに付与できます。
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
$ sudo insmod klfer.ko
```

### klferctl(アプリケーション)使い方

usageを```-h```オプションで出力できます。

```
$ sudo ./klferctl -h
Usage:
  klferctl { -A <FUNC_NAME> [OPTIONS] | -D <FUNC_NAME> | -R | -e | -d | -p | -h }

    -A     Add new function to be registered
    -D     Delete registered function
    -R     Reset all registered functions
    -e     Enable logger
    -d     Disable logger
    -p     Print registered functions
    -h     Help

  FUNC_NAME:
    function name to be logged. MUST be symbol in kernel

  OPTIONS:
    -g <GROUP_NAME>  Group name to display in the log (default: "NO_GRP"
    -t               Whether to show the timestamp in the log (default: no)

  TEST COMMAND:
    -T     Call sample function (klfer_sample_func)
 ex) # klferctl -T
```

まずサンプル関数を登録します。

```
$ sudo ./klferctl -A klfer_sample_func -g "SAMPLE_GRP" -t
$ sudo ./klferctl -A klfer_sample_nested_func -g "SAMPLE_GRP" -t
```

登録した関数は```-p```オプションで確認できます。

```
$ sudo ./klferctl -p
[Func] function_name                    [Grp] [TS]
[   0] klfer_sample_func                [  0] [ y]
[   1] klfer_sample_nested_func         [  0] [ y]
```

関数登録後、ログを有効化します。

```
$ sudo ./klferctl -e
```

DebugモードでBuildすると、テストコマンド```-T```でサンプル関数を実行できます。 
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
$ sudo ./klferctl -T
```

ログを確認します。

```
$ dmesg
[ 1625852088291388526 nsec] SAMPLE_GRP[0] e klfer_sample_func
enter klfer_sample_func()
[ 1625852088291394477 nsec] SAMPLE_GRP[1] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[ 1625852088291395901 nsec] SAMPLE_GRP[2] r klfer_sample_nested_func
[ 1625852088291396742 nsec] SAMPLE_GRP[3] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[ 1625852088291397684 nsec] SAMPLE_GRP[4] r klfer_sample_nested_func
[ 1625852088291398345 nsec] SAMPLE_GRP[5] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[ 1625852088291399225 nsec] SAMPLE_GRP[6] r klfer_sample_nested_func
[ 1625852088291399863 nsec] SAMPLE_GRP[7] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[ 1625852088291400770 nsec] SAMPLE_GRP[8] r klfer_sample_nested_func
[ 1625852088291401401 nsec] SAMPLE_GRP[9] e klfer_sample_nested_func
enter klfer_sample_nested_func()
[ 1625852088291402305 nsec] SAMPLE_GRP[10] r klfer_sample_nested_func
[ 1625852088291402980 nsec] SAMPLE_GRP[11] r klfer_sample_func
```

ログのフォーマットは、
```
[ <TIMESTAMP> nsec] <GROUP_NAME>[Sequence No.] <e(Entry)|r(Return)> function_name
```
となります。

ログを無効化します。

```
$ sudo ./klferctl -d
```

### LKMアンインストール

```
$ sudo rmmod klfer
```
