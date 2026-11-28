# Python 脚本排班

将自定义 `.py` 脚本放入 `RabbitRoll.exe` 同级的 `scripts` 文件夹，再在程序设置中选择。

本功能旨在允许用户提供高度自定义的排班逻辑。

在设置中开启【使用脚本排班】，并选择一个脚本，程序自带 [example.py](example.py)。回到编辑界面，点击【一键排班】，选择【使用 Python 脚本排班】，再点击【确定】，程序将按照 Python 脚本的排班逻辑执行排班。

脚本必须包含以下入口：

```python
def generate_schedule(api, target_per_cell):
    # 返回 {任务标识: [成员 ID, ...]}
    ...
```

其中，`api` 用于访问程序提供的 API 接口，`target_per_cell` 指在【一键排班】界面中设置的单次排班人数。

为避免重名导致程序错误，所有成员底层都是通过 ID 确定。可以通过 `api.get_task_candidates()` 获取程序预先筛选好的候选人名单，通过 `api.get_member_duty_counts()` 获取所有成员已被排班次数。具体写法可参考自带脚本。

在设置中启用【打开控制台】，即可通过 `api.debug()` 打印调试信息。

## 脚本入口与返回值

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

## Python 脚本可调用的 API

以下接口说明以当前代码实现为准。除 `set_member_property()` 外，获取数据的接口返回的是副本，修改返回的字典或列表不会直接修改程序中的数据。

### 获取候选人名单

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

### 获得全部成员列表

```python
api.get_all_members() -> list[str]
```

无参数。获取系统中所有成员的 ID 列表，顺序与传入脚本的成员列表一致。

若您想完全按照自己的逻辑排班，不接受程序计算好的候选人名单，可以通过这个方法获取所有成员。此时需要自行判断成员是否可以值班。

```python
member_ids = api.get_all_members()
```

### 根据姓名查找成员

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

### 获取成员属性

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

### 获得单个成员已被排班次数

```python
api.get_member_duty_count(id) -> int
```

参数 `id` 为成员 ID 字符串。获取该成员在本次脚本执行开始时已有的值班次数；未找到对应记录时返回 `0`。

```python
count = api.get_member_duty_count(member_id)
```

### 获得所有成员已被排班次数

```python
api.get_member_duty_counts() -> dict[str, int]
```

无参数。获取所有成员的值班次数映射表，格式为 `{"成员ID": 次数}`。需要统计多位成员时，可一次获取后使用。

```python
counts = api.get_member_duty_counts()
count = counts.get(member_id, 0)
```

这两个次数接口提供的都是脚本执行开始时的数据。脚本在本轮排班中选定成员后，需要自行更新本地计数；再次调用接口不会自动计入尚未返回的排班结果。

### 获取成员的个人时间表

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

### 修改成员的属性值

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

### 调试与控制台

```python
api.debug(msg) -> None
```

参数 `msg` 为需要打印的信息，类型为 `str`，无返回值。在设置中启用【打开控制台】，即可查看脚本输出的调试信息；未打开程序控制台时，信息会输出到程序调试日志。

```python
api.debug("开始排班")
api.debug(f"当前共有 {len(api.get_all_members())} 位成员")
```

## 脚本运行环境

程序使用随包提供的 Python 3.12 环境。当前脚本允许导入的模块为 `random`、`math`、`datetime`、`json`、`copy`、`re` 和 `rabbit_api`，其中 `random`、`datetime` 已由程序预先提供。

脚本不能直接使用 `open()`、`eval()`、`exec()`、`compile()`、`globals()`、`locals()`、`getattr()`、`setattr()` 和 `delattr()` 等被移除的内置函数，也不能直接导入允许列表之外的模块。
