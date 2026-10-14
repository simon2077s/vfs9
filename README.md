# vfs9

VFS9 是一个 C++ 虚拟文件系统库。与其它虚拟文件系统库的不同之处在于，vfs9 不仅实现了对本地文件系统的挂载，其设计目标还包括挂载归档文件以及各类功能性子系统 —— vfs9 在设计时充分参考了 plan9 的设计思想，将一切功能均抽象为对文件系统的操作，这也是本库被命名为 vfs9 的原因。

举例：我们可以将程序的日志子系统挂载到 `/log` 中，那么此后我们可以通过写入 `/log/<module_name>/text` 文件来写入日志、通过写入 `/log/<module_name>/config/buffer_size` 来修改该模块日志的缓存大小（类似sysfs的做法）。通过文件系统抽象一切查询和调用的方法来简化接口设计，这种设计方法并非性能上最高效的（因此在一个足够内聚的模块内部，你应当尽可能的通过直接调用的方式来使用其他子模块的功能而非通过 vfs9），其设计目的是尽可能降低插件化功能、调试诊断和指令干预类功能的开发、适配代价。通过标准化的文件访问接口你几乎不需要为新增的子系统适配插件接口、SDK等（当然你仍然需要适配子系统的 vfs9 接口）。

不过这里确实存在“例外情况”，vfs9 为了更好的性能在 “一切皆文件” 的基础上做了更多扩展定义，这些定义的目的是提高性能但相应的也会破坏原文件系统抽象的一致性约束，从而带来了额外的适配成本，这一部分主要在对 “文件” 概念的特化上。

特别感谢：https://copy.sh/v86/?profile=9front 提供的模拟环境，几乎所有路径查找、挂载和绑定能力都在模拟环境中进行了对比，以保证 vfs9 和 9front 的处理逻辑是一致的。

## 当前状态

vfs9 目前处于早期开发阶段，已实现的后端包括：

- `memfs` —— 纯内存文件系统，支持目录、placeholder / regular / scalar 文件，主要用于测试与临时数据。
- `localfs` —— 将宿主机上的一个绝对路径目录映射为文件系统，只暴露目录与 regular 文件（符号链接会被跟随，scalar / control 文件不支持）。

`archivefs`（归档文件后端）尚未实现。`vnamespace`（见 **核心类型**）已经暴露完整的基于路径的文件系统接口：查询、创建、删除、打开文件以及读写 / 执行辅助函数。

## 构建

编译依赖：

- **CMake >= 4.0**
- **C++23** 编译器：代码大量使用 `std::expected` 等 C++23 特性
- **Boost**：需要 `program_options`、`json` 组件
- **llfio**：推荐通过 vcpkg 安装
- **Threads**

测试与基准依赖由 CMake 通过 `FetchContent` 自动拉取：Catch2 v3.5.2（测试）、Google Benchmark v1.9.5（基准）。

仓库提供了读取 `VCPKG_ROOT` 环境变量的 vcpkg configure preset：

```sh
# 需提前设置 VCPKG_ROOT 环境变量
cmake --preset vcpkg -DVFS9_BUILD_BENCHMARKS=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

可用的 CMake 选项：`VFS9_BUILD_TESTS`（默认 ON）、`VFS9_BUILD_BENCHMARKS`（默认 ON）。公共总头文件为 `src/vfs9.hpp`。

## 核心行为

借鉴 plan9 的设计，vfs9 的核心行为包括：

- mount - 将文件系统挂载到虚拟文件系统中
- bind - 将某个路径的节点绑定到另一个路径上（目录与文件均支持）
- union directory - 联合目录，一个虚拟节点下可以挂载、绑定多个节点，对该节点进行查询时按优先级首先命中的节点会被使用

用一些例子更具体的描述这些行为：

### `mount fs old`

该操作将 fs 挂载到 old 目录下，必须满足以下条件挂载才能成功

- old 是一个有效的目录
- old 不能处在只读状态

挂载规则以路径的形式记录在挂载表中，因此其可见性与路径能否被解析相关（详见 **不追踪节点以及不支持移动语义**）。读取、写入 old 路径的行为请看 **读取和写入**

### `bind new old`

该操作将 new 路径的节点（如果是联合目录则是所有节点）添加到 old 路径的节点列表中，必须满足以下条件绑定才能成功

- old 是一个有效的目录或文件
- new 是一个有效的目录或文件
- old 不能处在只读状态
- new / old 必须同时是目录或文件，不能一个是目录一个是文件

需要特别注意的是，bind 记录的是 new 路径对应的节点引用（文件系统 + 在文件系统内的路径），而 vfs9 在解析路径时依据挂载表中记录的路径而非节点标识进行跳转（详见 **不追踪节点以及不支持移动语义**）。因此绑定建立之后，如果 new 对应的节点被删除或移动，访问 old 会解析到已经不存在（或已改变）的路径，从而失败或得到不同的结果 —— 这一点与 plan9 不同，plan9 记录的是节点，因此移动/删除后仍可访问到原节点。同时，绑定规则本身记录在 old 路径对应的挂载项上，删除或移动 old 节点并不会移除该规则。

### 读取和写入

`mount` 与 `bind` 在读取和写入上的行为是一致的。我们首先介绍合并标记参数，该标记决定了合并行为，其有三个值：

- Replace 替换合并（最常用）。指定该标记后 new（或 fs）将会完全替换 old 路径，原 old 路径的内容将会被遮蔽，如果此前 old 路径已经绑定了其它路径或 fs，那么这些绑定也会被遮蔽。
- Before 将 new 放在节点列表的最前面，也可以理解为绑定的节点或 fs 具有最高优先级。此时访问 old 路径时 vfs9 会合并 old 路径下所有节点对应的信息，如果存在同名子节点，那么高优先级的节点会遮蔽低优先级的。
- After 将 new 放在节点列表的最后面，即绑定的节点或 fs 具有最低优先级。

此外，`before` / `after` 只能用于目录（或联合目录）之间的合并，且 new / old 必须同为目录；只有 `replace` 同时支持文件与目录。

当写入 old 路径时（比如创建子目录或文件）的行为如下：

- 如果 old 路径只有唯一一个节点，那么则会尝试在该节点上进行写入
- 如果 old 路径有多个节点，那么 vfs9 会按照优先级从高到低查找第一个在绑定或挂载时指定了可写入标记的节点，找到该节点后尝试进行写入
- 如果 old 路径有多个节点，且挂载、绑定的节点都不可写入，同时 old 原始节点没有被遮蔽则会尝试写入 old 原始节点（原始节点默认是可写入的）

如果最终选中的目标节点不可写入（例如该节点来自一个以只读方式挂载或绑定的文件系统），则写入会失败并返回 `errc::unsupported`。

### 单值文件

Scalar file 是 vfs9 针对保存单个值类型的文件所做的读写优化，设计这个类型的灵感来源于 linux sysfs —— 通过直接保存、读取和写入原始值类型，避免通过 binary / string 进行转换或序列化、反序列化新增额外的性能成本。scalar file 的值以 `std::any` 保存，因此在关闭 RTTI 的编译配置下无法使用 scalar file（regular 文件不受影响）。

### 冻结系统

这个库的设计初衷是针对游戏引擎场景而设计的，而游戏场景有一个明确的特点就是在完成资源初始化后不会再进行挂载、绑定上的变更，因此 vfs9 提供了一个 freeze 接口，文件系统被冻结后会优化锁的使用方法等以提升性能。

在冻结文件系统后以下操作是不被允许的（统一返回 `errc::frozen`）

- `mount` / `unmount` 无法再修改挂载状态
- `bind` 无法再修改绑定状态（解除绑定同样通过 `unmount`）
- `create_directory` / `create_file` 无法再创建新的目录或文件
- `remove` 无法再删除目录或文件

查询（`status` / `exists` / `get_info` / `list`）以及文件的打开与读写不受冻结影响。

注意：不存在 unfreeze 这样的操作，一旦冻结后就只能处于冻结状态

### 不追踪节点以及不支持移动语义

plan9 通过 qid 来唯一的追踪一个目录或文件，这样使得目录或文件被移动不会影响对该文件的别名访问时的行为，但是 vfs9 是一个用户态的 C++ 库，其没有办法准确的跟踪目录、文件的变化 —— 虽然主流平台（windows / linux / macos）各自提供了不同的方法获得唯一标识符以及使用该标识符进行访问，但是这一部分工作的跨平台效果、对系统版本的要求和复杂度已经严重超出 vfs9 第一阶段的设计目标；同时即使是在纯粹的虚拟文件系统中支持节点追踪也需要实现复杂的节点引用和管理机制，造成API接口变得非常复杂，因此这里必须说明 vfs9 目前是不追踪本地文件系统、虚拟文件系统的节点变更的，这一点和 plan9 的行为是有根本性的差异的。

并且由于这个差异，vfs9 带来了一个和 plan9 在行为上的关键分析：**vfs9 在解析路径时使用挂载表记录的路径而非引用节点进行跳转**。这个设计分歧从根本上造成了 vfs9 与 plan9 在语义上出现差异，尤其是对于熟悉 plan9 的工程师来说理解这个分歧尤其重要。我们看如下的例子：

```bash
mkdir -p /mnt/server0;
mount memfs /mnt/server0;
mkdir /mnt/server0/data;
bind /mnt/server0/data /home/data;
mv /mnt/server0/data /mnt/server0/data1;
```

假设上述指令可以在 vfs9 中成功运行，那么基于上述提到的这个设计分歧，访问 `/home/data` 将被重定向到 `server0` 对应的文件系统的 `/data` 目录，而该目录在经过最后一条指令后已经不存在了，因此该访问会返回错误，但在 plan9 中，因为绑定记录的是节点而非路径，因此该访问依然有效且可以正确的访问到 `server0` 对应的 `/data1` 目录。

同时鉴于 vfs9 不支持节点追踪，在 vfs9 中支持移动语义会造成极大的困惑，我相信引入移动语义造成的混乱要远远大于其收益，拒绝移动语义可以在一定程度上避免混乱发生。如果你对这一部分逻辑感兴趣那么请阅读挂载表、路径解析部分的实现。

## 核心类型

- `vnamespace` —— 虚拟命名空间，vfs9 的核心类型。所有挂载、绑定、路径解析都围绕 `vnamespace` 展开，通过 `make_namespace()` 创建。它持有一个根文件系统节点（默认是一个空的 memfs）以及本地的挂载表。
  - `mount(path, merge_flag, writable, fs_params, ...)` 将文件系统挂载到指定路径；`merge_flag` 取值为 `replace` / `before` / `after`
  - `bind(new_path, old_path, merge_flag, writable)` 将新路径绑定到旧路径
  - `unmount(...)` 解除挂载 / 绑定
  - `clone()` 克隆出一个独立的 standalone 命名空间
  - `shadow()` 创建一个继承父命名空间的 shadow 命名空间（父命名空间始终只读）
  - `freeze()` 冻结命名空间；冻结后 `mount` / `bind` / `unmount` / `create_directory` / `create_file` / `remove` 均返回 `errc::frozen`
  - 查询：`status` / `exists` / `get_info` / `list`
  - 变更：`create_directory` / `create_file` / `remove`
  - 文件：`open_file`（stream / seq_stream / mapped / scalar / control 五种句柄）以及 `read_all` / `write_all` / `read_scalar` / `write_scalar` / `execute` 辅助函数
- `detail::node_id` —— 由 (fs_id, 文件系统内路径) 组成，是挂载表的键。vfs9 依据挂载表中记录的路径而非节点标识进行跳转（见上文“不追踪节点”）。
- `detail::mount_point` / `detail::mount_entry` —— 挂载点，包含一个有序的挂载项列表，当列表项多于一个时构成联合目录，列表顺序即优先级；挂载项还带有可写标记，写入时会选择第一个可写项。
- `detail::file_system` —— 类型擦除的文件系统门面，`make_file_system(params, args...)` 依据 `fs_impl<T>` 特化构建具体实现并完成初始化。
- `memfs` / `localfs` —— 内置的文件系统后端：内存文件系统与本地文件系统。新增后端只需提供参数类型 `T` 并特化 `detail::fs_impl<T>`，使其实现满足 `is_fs` 契约。

## CLI

vfs9_cli 是一个独立可运行的二进制程序，用于测试、实验 vfs9 行为或作为工具来读取 vfs9 的归档格式或单纯的用于解包 vfs9 支持的包类型文件。

所有命令解析都基于 `boost::program_options` 实现，收入依赖 `cpp-linenoise` 库来处理输入文本。

CLI启动命令参数：

- `-t` `--tracer` 启动tracer，**当前只留作接口，尚未实现**
- `-c` `--config` 加载配置文件，**当前只留作接口，尚未实现**
- `-h` `--help` 显示帮助

CLI内命令的所有路径参数可以是绝对路径也可以是相对路径，如果相对路径则拼接上当前路径得到绝对路径，CLI启动时当前路径为`/`

启动后可以在 `>` 提示符后输入命令，支持的命令包括：

- `ls [path]`

  列出 path 对应的目录的所有子项，如果未提供 path 参数则列出当前目录，列出的信息包括：子项名 和 类型

- `cd [path]`

  进入 path 对应的目录，如果未提供 path 参数则该命令无效果

- `pwd`

  输出当前（绝对）路径

- `mkdir path [-p]`

  在 path 位置创建一个文件夹，指定 -p 后会递归的创建

- `mount fs_name:name:args old_path [-a][-b][-w]`

  挂载一个文件系统，fs_name 是 vfs9 支持的文件系统的名称；name是该挂载的名称（用于unmount时指定）；args是:分割的参数，参数格式由各个文件系统定义：
  - memfs 没有参数
  - localfs 参数是一个路径，该路径作为localfs的根目录

  old_path 参数是文件系统要挂载的位置，该位置必须为一个已存在的文件夹或联合目录。

  可选参数：
  - -a 创建一个联合目录（或使用当前联合目录），并将文件系统挂载到目录的最后（最低优先级）
  - -b 创建一个联合目录（或使用当前联合目录），并将文件系统挂载到目录的开头（最高优先级）
  - -w 指定该挂载目录为新的联合目录的可写项，只有在使用 -a 或 -b 时才有意义

  如果没有指定 -a 或 -b，old_path指向的目录会被新文件系统遮蔽。

- `bind new_path old_path [-a][-b][-w]`

  将 new_path 对应的节点绑定到 old_path 上，参数与 mount 语义一致，该命令的要求是：
  - new_path 和 old_path 都要存在
  - new_path 和 old_path 对应于同一个类型（都是文件，或者都是文件夹或联合目录）

- `unmount name/new_path old_path`

  卸载 old_path 上 name/new_path 对应的挂载或绑定项。

- `unmount-all [old_path]`

  卸载 old_path 上所有挂载项或绑定项，如果不指定 old_path 则卸载 vnamespace 的所有挂载或绑定项。

- `exit` / EOF 退出CLI
