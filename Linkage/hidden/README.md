# HIDDEN

这里测试符号的可见性

## 步骤

分为`file1.cpp`和`file2.cpp`两个文件，也既不同的`object`

通过命令：`g++ --shared -fPIC -o library file1.cpp file2.cpp`

生成一个动态库`library`

（为了方便，这里跳过分别显式构建`object`再生成动态库）

## 有什么问题

这里特殊在于`file2.cpp`中的`api()`必须用`file1.cpp`的实现`mustBeKnownButNotAnAPI()`

`api()`符号是我期望暴露给调用方的

而`mustBeKnownButNotAnAPI()`我并不希望暴露出去（注意这种情况`static`救不了，我就是要跨`object`调用）

但是矛盾在于，因为`api()`需要调用`mustBeKnownButNotAnAPI()`，但它不在同一个`object`上，因此后者也要暴露出去

## 怎么看

通过命令：`readelf -s library > library_symbol`

生成了ELF中的符号信息，可以看到

```
    47: 0000000000001119    11 FUNC    GLOBAL DEFAULT   12 _Z22mustBeKnownButNotAnAP
    48: 0000000000001124    16 FUNC    GLOBAL DEFAULT   12 _Z3apiv
```

两个符号都是`GLOBAL`绑定，并且可见性都是`DEFAULT`，也就是看得见的意思

很显然，这个`_Z22mustBeKnownButNotAnAP`不应该被外部看到

## 尝试处理

这里试了两种方案：
- `file3.cpp`和`file4.cpp`，生成`hidden_library`和`hidden_library_symbol`
- `file5.cpp`和`file6.cpp`，生成`hidden_library2`和`hidden_library2_symbol`

### 方案一

第一种方案是把默认可见性全局设为`HIDDEN`，在`api()`添加`__attribute__((visibility("default")))`

构建命令：`g++ -fvisibility=hidden --shared -fPIC file3.cpp file4.cpp -o hidden_library`

符号输出命令：`readelf -s hidden_library > hidden_library_symbol`

此时在`.dynsym`中已经找不到`_Z22mustBeKnownButNotAnAP`

并且可以在`.symtab`里对比出不同

```
    42: 00000000000010f9    11 FUNC    LOCAL  DEFAULT   10 _Z22mustBeKnownButNotAnAP
    46: 0000000000001104    16 FUNC    GLOBAL DEFAULT   10 _Z3apiv
```

`_Z22mustBeKnownButNotAnAP`的绑定方式是`LOCAL`

> 注：
>
> 如果仅是生成object而不是直接生成library，则是`GLOBAL`加上`HIDDEN`，见`file3_elf`文件，它是由`file3.o`导出的（同样使用`-fvisibility=hidden`）
>
> 也就是说，`object`和`lib`中符号的绑定和可见性是不一样的

### 方案二

其实也差不多，只是在`mustBeKnownButNotAnAPI()`加上`__attribute__((visibility("hidden")))`

构建命令：`g++ --shared -fPIC file5.cpp file6.cpp -o hidden_library2`

符号输出命令：`readelf -s hidden_library2 > hidden_library2_symbol`

结果也是和方案一的输出相同

索然无味。。
