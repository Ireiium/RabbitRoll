# RabbitRoll兔子排班

RabbitRoll兔子排班是一款专注于解决团队排班问题的软件，尤其适用于大学生团队管理。它能够高效处理诸如高校学生会成员值班安排等繁琐事务，显著减少工作量。

程序支持平台：Windows。

## 构建说明

### 构建环境

- **操作系统**：Windows x64。
- **Qt**：Qt 6.8.3，需安装 Core、Widgets、Concurrent、Network 模块。
- **编译器**：MSVC 2022 x64，搭配 Qt 的 `msvc2022_64` 套件。
- **构建工具**：Qt Creator，或 CMake GUI 配合 Visual Studio；CMake 版本需为 3.19 或更高。
- **Python**：编译源码需要完整的 64 位 Python 3.12 开发环境，包含解释器、开发头文件和链接库。

仓库中的 `libs` 目录提供 ElaWidgetTools、QXlsx 和 pybind11，`pythonembed/Windows` 已包含运行时使用的 Python 环境。运行构建、打包后的程序及排班脚本，无需另外安装 Python。

### 使用 Qt Creator 构建

1. 使用 Qt Creator 打开项目根目录的 `CMakeLists.txt`。
2. 选择 **Qt 6.8.3 / MSVC 2022 x64** 构建套件。
3. 在 CMake 配置中确认 Python 3.12 的路径；若无法自动找到，可设置 `Python3_ROOT_DIR` 为 Python 3.12 的安装目录。
4. 根据需要设置 `ENABLE_AI`，重新运行 CMake，再构建项目（见**开启或关闭 AI 排班**）。

### 使用 CMake GUI 和 Visual Studio 构建

1. 打开 CMake GUI，在 **Where is the source code** 中选择源码主根文件夹，即 `CMakeLists.txt` 所在文件夹。在根目录创建 `build` 文件夹，并在 **Where to build the binaries** 中选择该文件夹。
2. 点击【Add Entry】，名称输入 `CMAKE_PREFIX_PATH`，类型选择 `PATH`，Value 选择 Qt 文件夹下的 `6.8.3/msvc2022_64` 文件夹，最后点击【OK】。
3. 点击【Configure】，选择 **Visual Studio 17 2022**，平台选择 **x64**，再点击【Finish】。若提示未找到 Python 开发环境，添加 `Python3_ROOT_DIR`，指向完整的 Python 3.12 安装目录，再次点击【Configure】。
4. 如需 AI 排班，确保勾选 `ENABLE_AI`；不需要则取消勾选。确认配置成功后，点击【Generate】。
5. 完成后点击【Open Project】，进入 Visual Studio。
6. 将 `RabbitRoll` 设为启动项目，并切换为 **Release / x64** 配置。
7. 选中 `RabbitRoll` 项目，按下 **Ctrl+B** 开始编译，也可以右键项目选择【生成】。使用上述构建目录时，当前源码的程序输出位置为 `build/src/Release/RabbitRoll.exe`。

当前 CMake 默认会在构建后自动部署 Qt 运行库，并复制 ElaWidgetTools 动态库、Python 环境、脚本、Excel 模板和许可证文件。若程序提示缺少 `ElaWidgetTools.dll`，可将 `libs/ElaWidgetTools/bin` 中的对应文件拷贝至 `RabbitRoll.exe` 所在文件夹。也可以使用 Qt Creator 打开项目，完成构建后直接运行。

### 打包与部署

若需将程序分发至其他设备，可以在 CMake GUI 中设置 `CMAKE_INSTALL_PREFIX`，重新配置、生成后，在 Visual Studio 中构建 `INSTALL` 项目。CMake 会将程序、运行库和资源部署到指定目录。

也可以按以下步骤，使用 Qt 的 `windeployqt` 工具手动打包：

1. 将编译生成的 `RabbitRoll.exe`、`python312.dll`、`ElaWidgetTools.dll`，以及 `scripts`、`pythonembed`、`Excel template` 和 `licenses` 文件夹，拷贝至一个独立的空文件夹，例如 `D:/RabbitRoll`。这些文件应取自同一次 Release 构建的输出目录。
2. 进入本次构建所用 Qt 的 `6.8.3/msvc2022_64/bin` 文件夹，右键点击【在终端中打开】，输入以下部署命令并回车。请将示例路径替换为实际的程序路径。

```powershell
.\windeployqt.exe --release "D:\RabbitRoll\RabbitRoll.exe"
```

完成后，程序打包完成，可直接打开 `RabbitRoll.exe` 运行。分发时请保留整个打包目录中的运行库和资源文件。

### 开启或关闭 AI 排班

CMake 选项 **`ENABLE_AI`** 控制是否将 AI 排班功能编译进程序，默认值为 `ON`。选项定义在 `src/CMakeLists.txt` 中：

```cmake
option(ENABLE_AI "Build with DeepSeek AI scheduling support" ON)
```

| 配置 | 构建结果 |
| --- | --- |
| 勾选 `ENABLE_AI`（`ON`） | 编译 AI 服务、DeepSeek 接口及 API key 存储代码，提供 AI 排班功能和相关设置。使用前仍需用户自行填写 API key。 |
| 取消勾选 `ENABLE_AI`（`OFF`） | 不编译上述 AI 服务代码，移除相关设置项，不提供 AI 排班能力；普通排班和 Python 脚本排班仍可使用。 |

**开启 AI 支持后**，CMake 会定义 `ENABLE_AI_SUPPORT` 编译宏，并为 AI 功能链接 Qt Network 和 Windows Crypt32。用户需进入【设置】→【启用 AI 排班】，打开功能开关，填写自己的 DeepSeek API key，按回车或离开输入框保存，再使用【AI 排班】。构建时开启 AI 支持不会自动提供 API key，也不会自动打开程序内的 AI 开关。

**开启或关闭 AI 支持的操作方式：**

- **Qt Creator**：在项目的构建设置中找到 CMake 配置项 `ENABLE_AI`，勾选或取消勾选，重新运行 CMake 后构建。
- **CMake GUI + Visual Studio**：在 CMake GUI 中勾选或取消勾选 `ENABLE_AI`，依次点击【Configure】和【Generate】，再回到 Visual Studio 重新构建。

`ENABLE_AI` 是编译选项，关闭后无法仅通过程序设置恢复 AI 功能，需要重新构建开启版。关闭 AI 后，当前 CMake 构建仍需要 Qt Network 和 Python 3.12 开发环境。

## 使用 RabbitRoll 的工作流

在高校团队值班管理中，通常由负责排班的成员收集部门所有成员的时间表，再一一核对每个成员可值班时间，协调出一份可执行的值班表。高校团队中，成员是否可值班往往取决于课程安排。

若团队成员规模大，手动核对每个成员可值班时间并协调各成员是一件极其耗时且效率低下的事。因此，RabbitRoll 提供了这样的解决方案：

1. 部门成员使用浏览器插件 **RabbitRoll 课表读取助手**，获取自己的课表。
2. 将读取的 `.rsch` 文件发送给部门负责排班的同学（下简称“负责人”）。
3. 负责人使用 RabbitRoll 程序读取 `.rsch` 文件后，即可一键排班。

您也可以直接在程序中编辑成员的个人时间表。

## 基本操作

刚启动程序时只有一个【主页】选项，创建或打开值班表后，即可看到其他工作按钮。

- **文件**：文件保存和打开功能。
- **生成**：用于导入与导出。
- **置顶菜单**：点击后，窗口会一直保持在电脑桌面的最上层。
- **主题切换菜单**：切换窗口主题，支持白天、黑夜主题。
- **左侧导航栏**：切换当前页面。

## 成员模块

创建或打开一个值班表后，点击【成员】即可进入成员界面。

- **添加与删除**：点击【添加成员】即可添加一个成员；选择一个或多个成员后，点击【删除成员】即可删除，也可以右键选中一行进行删除。
- **属性管理**：点击【管理属性】，可以在弹窗中添加或删除属性。
- **批量添加**：默认情况下，在编辑栏中一行一行输入成员姓名，点击【确定】即可导入。若需要同时导入部门、电话等属性，点击【添加格式】，输入预设的格式，每个属性之间需要用空格隔开。也可以从【文件导入】中导入 `.rmem` 文件进行批量添加。
- **导出成员**：点击【导出】，或通过【生成】→【导出成员列表】，将成员信息以 `.rmem` 格式导出；也可以通过【导出成员列表为 Excel】导出为 `.xlsx` 文件。

### 成员的个人时间表

双击一行的成员，即可编辑和查看成员的详细信息，以及导入和导出该成员的时间表。

在【成员信息】浮窗中，双击【个人时间表】的表格，即可标记该成员某一天某时刻不能值班。点击【添加时间段】和【添加日期】，即可添加时间、日期。

点击【导入时间表】，可以导入先前保存好的 `.rsch` 文件，或者使用 RabbitRoll 课表读取助手获得的 `.rsch` 文件；点击【导出时间表】，即可把编辑好的时间表以 `.rsch` 文件导出。

## 编辑模块

点击【编辑】图标，即可进入排班编辑界面。

### 模式切换与排班处理逻辑

点击【切换到日期模式】或【切换到星期模式】，日期显示会变成周几或年月日，对应的程序内部排班逻辑也会有相应的变化。

当您编辑好某成员的时间表后，程序后台会自动计算：该成员在哪一天、哪一个时间段可以值班，哪一天、哪一个时间段无法值班。

因此，在对某一天某个时间段排班时，程序会直接告诉您哪些成员可以值班。您只需双击这个单元格，然后在左侧浮窗中进行勾选即可。

在周期模式下，只要成员在某一周的某一天无法值班，默认情况下，系统就不会将其放入对应星期、时间段的可值班候选人中。您可以通过【不可值班容忍度】调整这一规则。

### 一键排班

点击【一键排班】，设置【单次排班人数】，即可一键完成排班选择。单次排班人数指一个时间段中一共排多少人。

**注意：无论是一键排班还是 Python 脚本排班，都会直接覆盖原来的排班表。**

### 排班表设置

- **排班表名称**：由文件名确定。
- **不可值班容忍度**：仅对周期模式生效。例如，甲在 4 月份的周三 10:35-11:30 中，有一个星期无法值班。设置为 `0` 时，候选人不会含有甲；设置为 `1` 或更高时，候选人会含有甲。
- **启用日期筛选范围**：设置程序选择候选人时，只在设定的日期范围内查找。

## RabbitRoll 课表读取助手

[下载链接](https://github.com/Ireiium/ClassScheduleReader_ForRabbitRoll)：点击`Code`-->`Download Zip` 下载后解压即可。

高校团队中值班的安排往往受到课程表的约束。因此，能够高效获取成员的课程表，是解决排班问题的重要步骤。

RabbitRoll 提供专门的课表读取助手浏览器插件，方便成员快速获取自己的课表。读取课表后以 `.rsch` 格式下载，可以直接导入 RabbitRoll 的个人时间表中。

本浏览器插件目前仅支持 **Edge 和 Google Chrome** 浏览器，目前适配的高校为 **武汉工程大学**。

### 导入与使用

1. 打开浏览器的扩展管理页面，开启【开发人员模式】。
2. 点击【加载解压缩的扩展】，找到并选择 `ClassScheduleReader_ForRabbitRoll` 文件夹，确保插件已开启。
3. 登录学校教务系统，进入自己的课程表页面。以武汉工程大学为例，进入【学期理论课表】，确保当前【周次】为全部。
4. 打开 RabbitRoll 课表读取助手，点击导出 `.rsch` 文件。若不成功，刷新页面重试。
5. 在 RabbitRoll 的成员个人时间表中导入该文件，选择好开学的第一天，点击【开始导入】。

## AI 排班功能

若有特殊的排班逻辑需要，可启用 AI 排班。

1. 进入【设置】，打开【启用 AI 排班】，填写 API key。
2. 进入编辑页面，点击【AI 排班】，输入要求，例如“把同一个部门的成员放在一起”。
3. 点击【选择属性】，把需要上传给 AI 的属性数据选中，例如“部门”，并确定。
4. 点击【开始计算】，完成排班。

## Python 脚本排班

本功能旨在允许用户提供高度自定义的排班逻辑。

在设置中开启【使用脚本排班】，并选择一个脚本，程序自带 [example.py](scripts/example.py)。回到编辑界面，点击【一键排班】，选择【使用 Python 脚本排班】，再点击【确定】，程序将按照 Python 脚本的排班逻辑执行排班。

脚本必须包含以下入口：

```python
def generate_schedule(api, target_per_cell):
    # 返回 {任务标识: [成员 ID, ...]}
    ...
```

其中，`api` 用于访问程序提供的 API 接口，`target_per_cell` 指在【一键排班】界面中设置的单次排班人数。

为避免重名导致程序错误，所有成员底层都是通过 ID 确定。可以通过 `api.get_task_candidates()` 获取程序预先筛选好的候选人名单，通过 `api.get_member_duty_counts()` 获取所有成员已被排班次数。具体写法可参考自带脚本。

在设置中启用【打开控制台】，即可通过 `api.debug()` 打印调试信息。

### 脚本入口与返回值

```python
def generate_schedule(api, target_per_cell):
    candidates = api.get_task_candidates()
    result = {}
    for task_key, member_ids in candidates.items():
        result[task_key] = member_ids[:target_per_cell]
    return result
```

以上为最简单的示例：按候选人列表顺序，为每个时间段选择指定人数，不进行值班次数均衡。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `api` | `rabbit_api.ScriptAPI` | 程序传入的接口对象，用于获取候选人、成员属性、个人时间表等数据。无需自行创建。 |
| `target_per_cell` | `int` | 在【一键排班】中设置的单次排班人数。脚本需要自行按此人数选择成员。 |

返回值必须是 `dict[str, list[str]]`，格式为 `{任务标识: [成员 ID, ...]}`。

- 任务标识必须来自 `api.get_task_candidates()` 的键，不能自行添加不存在的任务。
- 成员 ID 必须为程序中已有成员的 UUID 字符串，不能使用姓名代替。同一任务中重复的 ID 会被自动去重。
- 返回的字典会替换原来的排班结果，未返回的任务不会保留原来的安排；空列表表示该任务不安排成员。
- 程序会检查任务标识、成员 ID 和返回格式；是否使用预筛选候选人、每格人数以及其他排班规则，由脚本自行控制。

### Python 脚本可调用的 API

以下接口说明以当前代码实现为准。除 `set_member_property()` 外，获取数据的接口返回的是副本，修改返回的字典或列表不会直接修改程序中的数据。

#### 获取候选人名单

```python
api.get_task_candidates() -> dict[str, list[str]]
```

无参数。获取程序预先筛选好的候选人名单，格式为 `{ "模式_日期_时间段": ["成员ID", ...] }`。

任务标识格式为 **模式_日期_时间段**：`0` 表示周期模式，`1` 表示日期模式。例如：

- `0_周二_08:00-09:00`：周期模式下，周二 08:00-09:00。
- `1_2026-04-13_08:00-09:00`：日期模式下，2026-04-13 的 08:00-09:00。

某个任务没有可用候选人时，其成员列表为空。建议直接使用返回的任务标识，避免手动拼接。

```python
candidates = api.get_task_candidates()
for task_key, member_ids in candidates.items():
    api.debug(f"{task_key}：共 {len(member_ids)} 位候选人")
```

#### 获得全部成员列表

```python
api.get_all_members() -> list[str]
```

无参数。获取系统中所有成员的 ID 列表，顺序与传入脚本的成员列表一致。

若您想完全按照自己的逻辑排班，不接受程序计算好的候选人名单，可以通过这个方法获取所有成员。此时需要自行判断成员是否可以值班。

```python
member_ids = api.get_all_members()
```

#### 根据姓名查找成员

```python
api.find_member(name, index=0) -> str
```

为避免重名导致程序错误，所有成员底层都是通过 ID 确定。在脚本中若要对某一位成员修改属性或进行其他操作，需要先获得其 ID。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `name` | `str` | 成员姓名，与“姓名”属性进行精确匹配。 |
| `index` | `int` | 同名成员的序号，按成员列表顺序，**从 `0` 开始计数**。默认 `0`，即第一个同名成员。 |

返回该成员的 ID；未找到成员或序号超出范围时，返回空字符串 `""`。

例如成员列表为“张三、李四、张三、王五、张三”：

```python
first_id = api.find_member("张三")      # 第一个张三
second_id = api.find_member("张三", 1)  # 第二个张三
third_id = api.find_member("张三", 2)   # 第三个张三
```

#### 获取成员属性

```python
api.get_member_properties(id) -> dict[str, str]
```

参数 `id` 为成员 ID 字符串。返回指定成员的自定义属性字典，例如 `{"姓名": "张三", "部门": "开发部"}`，属性值均以字符串形式提供。成员不存在时返回空字典 `{}`。

```python
member_id = api.find_member("张三")
if member_id:
    properties = api.get_member_properties(member_id)
    department = properties.get("部门", "")
```

#### 获得单个成员已被排班次数

```python
api.get_member_duty_count(id) -> int
```

参数 `id` 为成员 ID 字符串。获取该成员在本次脚本执行开始时已有的值班次数；未找到对应记录时返回 `0`。

```python
count = api.get_member_duty_count(member_id)
```

#### 获得所有成员已被排班次数

```python
api.get_member_duty_counts() -> dict[str, int]
```

无参数。获取所有成员的值班次数映射表，格式为 `{"成员ID": 次数}`。需要统计多位成员时，可一次获取后使用。

```python
counts = api.get_member_duty_counts()
count = counts.get(member_id, 0)
```

这两个次数接口提供的都是脚本执行开始时的数据。脚本在本轮排班中选定成员后，需要自行更新本地计数；再次调用接口不会自动计入尚未返回的排班结果。

#### 获取成员的个人时间表

```python
api.get_member_schedule(id) -> dict[str, dict[str, bool]]
```

参数 `id` 为成员 ID 字符串。返回指定成员的个人时间表，格式为 `{"日期": {"时间段": 是否繁忙}}`。

- 日期格式：`yyyy-MM-dd`，例如 `2026-04-13`。
- 时间段格式：`HH:mm-HH:mm`，例如 `08:00-10:00`。
- **`True` 表示繁忙、不能值班；`False` 表示该时段未标记为繁忙。**
- 成员不存在或没有时间表数据时，返回空字典 `{}`。

```python
schedule = api.get_member_schedule(member_id)
busy = schedule.get("2026-04-13", {}).get("08:00-10:00", False)
if busy:
    api.debug("该成员在此时段不能值班")
```

该接口返回原始时间表，不会把时间段自动合并，也不会计算与查询时段的重叠关系。上例只读取完全相同的日期和时间段键；若要判断其他时段，需要自行处理时间重叠，或直接使用程序预先筛选好的候选人名单。

#### 修改成员的属性值

```python
api.set_member_property(id, key, value) -> None
```

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `id` | `str` | 要修改的成员 ID。 |
| `key` | `str` | 属性名称，例如“姓名”“部门”。 |
| `value` | `str` | 新的属性值，数值也需要转换为字符串。 |

修改或设置成员的属性值，无返回值。成员 ID 不存在时抛出 `ValueError`。修改后，本次脚本中调用 `get_member_properties()` 即可读取新值；脚本执行成功且返回格式校验通过后，程序会将属性修改写回成员模型。

```python
member_id = api.find_member("张三")
if member_id:
    api.set_member_property(member_id, "部门", "宣传部")
```

此接口用于修改成员属性，不用于直接修改个人时间表或排班结果。

#### 调试与控制台

```python
api.debug(msg) -> None
```

参数 `msg` 为需要打印的信息，类型为 `str`，无返回值。在设置中启用【打开控制台】，即可查看脚本输出的调试信息；未打开程序控制台时，信息会输出到程序调试日志。

```python
api.debug("开始排班")
api.debug(f"当前共有 {len(api.get_all_members())} 位成员")
```

### 脚本运行环境

程序使用随包提供的 Python 3.12 环境。当前脚本允许导入的模块为 `random`、`math`、`datetime`、`json`、`copy`、`re` 和 `rabbit_api`，其中 `random`、`datetime` 已由程序预先提供。

脚本不能直接使用 `open()`、`eval()`、`exec()`、`compile()`、`globals()`、`locals()`、`getattr()`、`setattr()` 和 `delattr()` 等被移除的内置函数，也不能直接导入允许列表之外的模块。

## 第三方许可说明与致谢

本项目采用 [MIT 许可证](LICENSE)。第三方组件遵循各自的许可证，相关许可文件见 [licenses](licenses/) 目录。

| 组件 | 许可证 |
| --- | --- |
| [Qt 6.8.3](https://www.qt.io/) | GNU Lesser General Public License v3.0 |
| [ElaWidgetTools](https://github.com/Liniyous/ElaWidgetTools) | MIT License |
| [QXlsx](https://github.com/QtExcel/QXlsx) | MIT License |
| [pybind11](https://github.com/pybind/pybind11) | BSD-style License |

本软件以动态链接方式使用 Qt 库。您有权在遵守 LGPLv3 协议的前提下，通过替换本软件目录下的 Qt 动态链接库文件，以实现测试、升级或修改的目的。您可以访问 Qt 官网获取完整源码。
