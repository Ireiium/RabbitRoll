# RabbitRoll MCP 调用说明

客户端可能需要手动加载本文。可将文件提供给 agent，或在任务中指定先阅读本文。

## 1. 连接与数据来源

- 客户端通过 **stdio** 启动独立的 `RabbitRollMcp.exe`，无需启动参数。建议将工作目录设为该程序所在的完整发布目录，保留随包提供的 DLL、插件和运行库。
- 先启动 `RabbitRoll.exe`，**MCP 随程序默认开启**，无需在设置中操作开关；辅助程序通过当前 Windows 用户、同一登录会话范围内的本地 IPC 访问 GUI。
- 多个 agent 可以各自启动辅助程序，它们访问同一个 RabbitRoll 主实例。查询和编辑针对 GUI 正在使用的内存数据，包括尚未保存的修改。
- 不要通过自己加载另一份 `.rroll` 文件来代替查询当前项目。业务文件路径使用绝对路径，避免依赖 agent 的工作目录。
- 辅助程序的 stdout 只承载 MCP 协议消息；日志使用 stderr。不要给启动命令增加向 stdout 打印提示的包装脚本。
- `start_rabbitroll` 可以请求启动同目录 GUI，返回 `already_running` 或 `launch_requested`。后者只表示启动请求成功；随后仍需确认 GUI 和本地业务服务已就绪。

本文不约定 Codex 或其他 harness 的配置文件格式。应按目标客户端的实际 stdio 配置方式，指定可执行程序的绝对路径、空参数列表和工作目录。

## 2. 首次调用顺序

1. 发现工具，读取服务器说明和工具描述。
2. 调用 `get_project_context`，判断当前项目、版本及 `data.availability_calculating`。
3. 有项目时，读取 `list_members` 和 `get_schedule`；编辑个人时间表前还要读取 `get_member_schedule`。
4. 无项目时，根据用户任务使用 `new_project` 或 `open_project`；切换已有项目时遵循保存和丢弃修改的规则。
5. 写操作前重新读取项目概况，使用最新版本；每次修改后等待空闲数据计算完成，再执行依赖它的后续操作。
6. 检查结果。需要排班时可先生成草案，审阅后再应用；应用后运行 `validate_schedule`。
7. 保存和导出使用对应工具，根据用户任务决定是否执行。内存编辑不会自动保存。

姓名可能重复，也可能在编辑中改变。定位成员使用工具返回的 UUID，不使用行号，不自行生成已有成员的 ID。

## 3. 参数、返回值与重试

### 3.1 两种参数形式

查询及部分控制工具直接接收字段。例如，`query_availability` 的 MCP 工具调用参数为：

```json
{
  "mode": "date",
  "target": "2026-11-20",
  "time_range": "08:00-10:00",
  "project_id": "<当前项目的 project_id>"
}
```

写工具接收一个 **字符串字段 `request_json`**。字符串内容是 JSON 对象，必须包含三个公共字段：

| 字段 | 类型 | 来源与要求 |
| --- | --- | --- |
| `project_id` | 字符串 | `get_project_context` 返回的顶层字段；标识当前项目会话，重新打开同一文件也会变化 |
| `expected_revision` | 字符串 | 当前概况的顶层 `revision`；保持字符串类型，不自行加一或转换为数值 |
| `request_id` | 字符串 | 每个新操作使用唯一值，建议 UUID，最长 128 字符；完全相同的请求重试时保持不变 |

例如，`manage_properties` 的实际工具参数为：

```json
{
  "request_json": "{\"project_id\":\"<project_id>\",\"expected_revision\":\"<revision>\",\"request_id\":\"<新请求的唯一ID>\",\"action\":\"add\",\"names\":[\"部门\"]}"
}
```

尖括号中的文本是待替换的占位符。不要将业务字段直接放到 MCP 参数顶层，也不要将 `request_json` 传成对象。

可以用以下纯数据封装函数构造参数；它不依赖具体客户端 SDK：

```python
import json
import uuid


def write_arguments(context, payload, request_id):
    request = dict(
        payload,
        project_id=context["project_id"],
        expected_revision=context["revision"],
        request_id=request_id,
    )
    return {"request_json": json.dumps(request, ensure_ascii=False)}


# context 是刚读取并解析的 get_project_context 结果。
request_id = str(uuid.uuid4())
arguments = write_arguments(context, {"action": "add", "names": ["部门"]}, request_id)
# 调用 manage_properties，传入 arguments。
# 如果只是重发这次请求，复用 arguments，不重新生成 request_id。
```

无项目时，`get_project_context` 返回 `ok:false`、`error.code:"no_project"`，同时提供 `project_id:""` 和字符串 `revision`。这些公共字段可以用于 `new_project`、`open_project`、`write_script`、`delete_script`；无项目时这四个工具也允许 `expected_revision:""`。不要把空版本当作已有项目的通用写入方式。

**下文写工具示例只展示业务 payload。** 发送前都要按上述方式补齐最新公共字段、序列化，并放入 `request_json`。

### 3.2 解析业务结果

工具结果以 JSON 放在 MCP 文本内容块中。解析后检查业务字段 `ok`：MCP 层调用成功时，业务操作仍可能返回失败。

常见成功结果：

```json
{
  "ok": true,
  "project_id": "<project_id>",
  "revision": "123",
  "data": {}
}
```

常见失败结果：

```json
{
  "ok": false,
  "error": {
    "code": "revision_conflict",
    "message": "项目已被修改，请重新读取数据后重试。"
  }
}
```

部分辅助工具只返回 `ok` 和 `data`，失败结果通常也没有版本字段。需要版本时重新读取 `get_project_context`。

### 3.3 请求重试

- 同一工具、同一 payload 和公共字段的完全相同请求，使用同一个 `request_id` 重试，避免重复新增成员或重复应用。
- 修改了 payload、项目 ID 或版本，就是一次新请求，使用新的 `request_id`。
- 去重记录只在运行中的 GUI 服务里有限保留，不能作为跨重启的永久去重保证。
- 收到 `timeout`、`disconnected` 或异常响应时，操作的执行状态可能尚未确认。先核查当前项目或已知 `job_id`，需要重发时复用原请求和 `request_id`。
- 多个 agent 可以并行读取；写操作串行推进，并处理其他 agent 或人工编辑带来的版本冲突。

## 4. 工具速查

以下直接参数表中，`?` 表示可选；无参数工具传 `{}`。所有写工具均使用前述 `request_json`。

### 4.1 直接参数工具

| 工具 | 直接参数 | 返回内容或作用 |
| --- | --- | --- |
| `get_project_context` | 无 | 项目名称、路径、修改状态、模式、日期范围、成员数、计算状态和两类撤销历史 |
| `list_members` | `project_id?` | `data.members` 中包含 `id`、`properties`、`note`、`enabled`；`data.properties` 为全局属性名称 |
| `get_schedule` | `project_id?` | 当前模式各单元格的 `target`、`time_range`、`member_ids` |
| `get_member_schedule` | `member_id` | 个人数据，包括 `scheduleDates`、`scheduleTimes`、`scheduleMap`、`note`、`enableScheduling` |
| `query_availability` | `mode`、`target`、`time_range`、`member_id?`、`project_id?` | 可用成员的稳定 `id` 与 `name` |
| `validate_schedule` | `project_id?` | `data.valid`、`issue_count`、`issues`；问题包含位置和原因 |
| `list_scripts` | 无 | GUI 程序所在目录的 `scripts` 中可运行的 `.py` 文件名 |
| `read_script` | `name` | 脚本 `code` 及当前内容的 `sha256` |
| `get_job_status` | `job_id` | 任务 `state` 和业务 `result` |
| `get_schedule_draft` | `draft_id` | 草案 `cells`、属性修改 `properties` 和基准参数 `based_on` |
| `cancel_job` | `job_id` | 取消运行中的 MCP 任务；已完成任务保持原状态 |
| `start_rabbitroll` | 无 | 请求启动 GUI，MCP 随 GUI 默认开启 |

查询日期使用 `yyyy-MM-dd`。`mode` 为 `date` 或 `weekday`；星期目标为 `周一` 至 `周日`，应与当前排班模式一致。时间段为同日 `HH:mm-HH:mm`，起点必须早于终点，不支持跨午夜的单个时段。

个人时间表中 **`true` 表示繁忙，`false` 表示空闲**，未标忙的时间按现有规则视为空闲。星期模式使用项目的搜索日期范围和容忍周数；未启用显式范围时，当前实现使用当天至三个月后。查询空闲不会排除已经分配的班次，安排后还要检查重叠冲突。

`validate_schedule` 检查当前模式下的失效引用、重复人员、禁用成员、个人繁忙冲突、重叠班次及无效位置。人数、公平性等未配置的要求需另行检查；`data.valid:true` 仅说明已检查的规则未发现问题。

### 4.2 项目与成员写工具

| 工具 | 业务 payload | 行为 |
| --- | --- | --- |
| `new_project` | `name`；`discard_unsaved?` | 新建空项目，名称 1–200 字符 |
| `open_project` | `path`；`discard_unsaved?` | 打开 `.rroll`，返回后台任务 |
| `close_project` | `discard_unsaved?` | 关闭项目，GUI 继续运行 |
| `manage_members` | `action`；`members` 或 `member_ids` | `add/update/delete`，批量操作可一次撤销 |
| `update_member_properties` | `changes:[{member_id,properties}]` | 仅批量修改已有属性，可一次撤销 |
| `manage_properties` | `action`、`names` | 全局添加 `add` 或删除 `remove` 属性，可撤销 |
| `edit_member_schedule` | `member_id`、`dates`、`time_ranges`、`cells` | 整体替换一个成员的个人时间表，可撤销 |
| `undo` | `scope` | 撤销一次，`scope` 为 `members` 或 `schedule` |
| `redo` | `scope` | 重做一次，范围同上 |

新建、打开和关闭默认拒绝丢弃未保存修改。根据用户已授权的意图先保存，或明确传 `discard_unsaved:true`；不要为绕过错误而自行改成 true。项目生命周期操作会清空撤销历史，不能用 `undo` 返回上一个项目。

新增成员的 `members` 项需要 `properties.姓名`，不传 `member_id`；UUID 由程序生成。修改项必须有 `member_id`，可提供 `properties`、`note`、`enabled`；删除使用 `member_ids` 数组，并清除相应排班引用。单次最多 500 人，姓名可以重复但不能为空。

属性值为字符串；属性名必须已经存在。需要新属性时先调用 `manage_properties`。全局属性名最多 8 字，不能有首尾空白、不能以下划线开头，保留 `姓名` 和 `序号`；不能添加或删除这两个保留名称。单次全局属性操作最多 100 个名称。

成员编辑类操作使用 `members` 撤销历史。排班配置、单元格分配、算法/脚本/AI 结果及草案应用使用 `schedule` 历史；脚本排班产生的属性修改也与该次排班一起撤销。先查看概况的 `data.history`，不要假定撤销范围覆盖两个历史栈。

### 4.3 排班写工具

| 工具 | 业务 payload | 行为 |
| --- | --- | --- |
| `configure_schedule` | `mode`、`columns`、`time_ranges`；`search_range?` | 设置当前模式的完整列及全表共用时间行，可撤销 |
| `assign_members` | `cells:[{target,time_range,member_ids}]` | 替换所列单元格的完整人员列表；空列表清空该格 |
| `auto_schedule` | `target_per_cell`；`prioritize_fewer?`、`allow_multiple?`、`preview?` | 使用现有一键排班算法，返回任务 |
| `run_schedule_script` | `script_name`、`target_per_cell`；`preview?` | 执行已有排班脚本，返回任务 |
| `ai_schedule` | `prompt`、`target_per_cell`；`thinking?`、`preview?` | 调用已配置的内置 AI，返回任务 |
| `preview_schedule` | `method`、`target_per_cell` 及对应算法的选项 | `method:auto/script/ai`，始终生成草案，返回任务 |
| `apply_schedule_draft` | `draft_id`；`cell_keys?` | 应用草案，可整次撤销；成功后该草案被移除 |

- `configure_schedule` 的 `columns` 在日期模式是日期数组，星期模式是星期数组；`time_ranges` 是完整时间行数组。遗漏原行列表示删除，可能清除相关分配，不是追加接口。日期/列最多 1000 项，时间行最多 100 项，乘积最多 10000 格。
- 可选 `search_range` 必须完整提供 `{enabled,start,end,tolerance_weeks}`，日期有效且起点不晚于终点，容忍周数为 0–5 的整数。不提供则保持原搜索设置。
- `assign_members` 只处理请求中列出的已有单元格，其他格保持不变；每格 UUID 不重复，最多 500 人，每次最多 1000 格。候选人必须通过当前项目的空闲和参与排班规则。
- `target_per_cell` 为 1–500 的整数，是算法的目标/上限参数；候选人不足时不能保证人数满足。自动排班的布尔选项默认 false；`allow_multiple` 对应星期模式允许成员多次参与。不要把次数少优先当作对所有既有历史分配的公平性保证。
- 非预览的自动、脚本和 AI 排班会替换当前模式的分配，结果未包含的原单元格分配也可能清空。需要保留其他格时，优先生成草案并使用 `cell_keys` 部分应用，或使用 `assign_members`。
- 草案中 `cells` 的键为 `模式_时间段_日期或星期`，例如 `1_08:00-10:00_2026-11-20`，其中 `0` 为星期、`1` 为日期。应用时直接采用返回的键。
- 草案基于生成时的项目和版本，后续修改使其过期。不要仅换成最新版本就强行应用旧草案，应重新生成。包含成员属性修改的脚本草案只支持整份应用。
- 内置 AI 需要启用 AI 的构建、界面开关和已配置的 API key；MCP 不提供读取或修改密钥的工具。调用会把候选人、成员属性和排班要求交给现有 AI 服务处理。关闭 AI 的构建仍可使用其他 MCP 功能。

### 4.4 文件和脚本写工具

| 工具 | 业务 payload | 文件与项目影响 |
| --- | --- | --- |
| `import_rsch` | `paths`；见下方导入选项 | 批量导入个人时间表，返回任务，可撤销 |
| `import_rmem` | `path`；`mode?` | 导入完整成员数据，返回任务，可撤销 |
| `export_excel` | `path`；`scope?`、`overwrite?` | 导出 `.xlsx`，返回任务，保持项目修改状态 |
| `export_rmem` | `path`；`overwrite?` | 导出完整成员 `.rmem`，返回任务，保持项目修改状态 |
| `export_rsch` | `member_id`、`path`；`overwrite?` | 导出一个成员的加密个人时间表 `.rsch`，返回任务 |
| `save_project` | `path?`、`overwrite?` | 保存完整 `.rroll`，返回任务；成功后更新路径、名称和保存状态 |
| `write_script` | `name`、`code`；`overwrite?`、`expected_sha256?` | 语法检查后创建或修改 `.py`，返回任务，不自动执行 |
| `delete_script` | `name`、`expected_sha256` | 删除脚本文件 |

文件格式与覆盖规则：

- `.rroll` 保存完整项目，包括成员、个人时间表、排班配置与分配；`.rmem` 保存完整成员信息，含 UUID、隐藏属性、备注、个人时间表及参与排班状态。
- **`.rsch` 保存个人忙闲时间表，不保存多人值班分配。** 导出多人值班结果使用 Excel 或保存 `.rroll`；导出 RSCH 后按文件名规则或显式 `targets` 选择重新导入的成员。
- 导出文件路径必须为绝对路径，目标目录须已存在；省略扩展名时程序补上对应扩展名，错误扩展名会拒绝。
- `overwrite` 默认 false。省略 `save_project.path` 表示保存到当前文件，允许覆盖；未保存项目必须指定路径。显式提供 `path` 一律按另存为处理，相同路径也遵循显式覆盖规则。
- 保存后的项目名称取文件名去掉扩展名。保存保留撤销历史；导出不会把项目标为已保存。
- 文件输出在完成时再次检查项目版本，项目切换、版本冲突或任务取消会阻止提交。成功写出的文件、删除的脚本不属于项目撤销历史。
- `export_excel.scope` 为 `schedule`（默认）、`members` 或 `both`；成员 Excel 使用界面可见属性列，完整成员备份应使用 `.rmem`。
- `import_rmem.mode` 为 `replace`（默认）或 `merge`。replace 替换成员集合；merge 按 UUID 更新同一成员、追加新成员，保留其他成员，不按姓名合并。`.rroll`/`.rmem` 输入文件当前限制为 64 MiB。

`import_rsch` 支持以下选项，一次最多 500 个不重复的绝对文件路径：

| 选项 | 含义与默认值 |
| --- | --- |
| `format` | 文件名属性格式，默认 `姓名`；属性名按空白分隔，例如 `部门 姓名` 对应 `技术部 张三.rsch`，必须包含一个 `姓名` |
| `semester_start` | 课程表格式转换使用的学期第一周周一，`yyyy-MM-dd`；相应文件需要提供有效周一 |
| `existing_only` | 默认 false；只导入已有成员时设 true |
| `replace_schedule` | 默认 false，按现有规则合并繁忙数据；true 表示替换目标成员个人时间表 |
| `overwrite_properties` | 默认 false；控制是否覆盖已有非空属性 |
| `targets` | `{绝对路径:成员UUID或"new"}`；显式指定已有成员或新建，解决重名/文件名匹配问题 |

任一 RSCH 文件校验或匹配失败时整批不导入。检查任务失败结果中的 `rows` 逐文件原因，修正后用新的请求 ID 提交，不忽略失败文件自行声称整批成功。

脚本文件位于 **GUI 的 `RabbitRoll.exe` 同级 `scripts` 目录**，不随 agent 工作目录改变：

- `name`/`script_name` 只接受 `.py` 文件名，不接受目录路径或任意文件访问。
- `write_script.code` 为 UTF-8 代码，最多 128 KiB；覆盖已有脚本需要 `overwrite:true` 和 `read_script` 最新返回的 `sha256`，作为 `expected_sha256` 提交。删除同样需要最新哈希。
- 写入只做 Python 语法检查，不保证存在正确排班入口或业务行为。排班入口为 `generate_schedule(api, target_per_cell)`，返回 `{单元格key: [成员UUID, ...]}`。
- 运行前列出或读取脚本；脚本可收集已有成员属性修改，与排班结果一起在成功时应用。失败不会应用收集到的内存修改。
- Python 沿用现有运行环境，不是沙箱。预览同样会执行脚本，外部文件、网络等脚本副作用不受项目撤销或草案机制保护；只运行任务所授权的可信脚本。
- 普通 Python 执行有 10 秒追踪限时和取消检查；阻塞 I/O、原生扩展不保证立即中止。不要把取消或超时描述成撤销已经发生的外部副作用。

## 5. 典型调用示例

本节所有业务 payload 都应使用第 3 节的封装方式发送。每步取得并检查结果，读取最新上下文后再写下一步；示例中的成员、草案 ID 和路径需替换成实际值。

### 5.1 导入、改名和维护属性

`import_rsch`：

```json
{
  "paths": ["D:/值班数据/技术部 张三.rsch", "D:/值班数据/技术部 李四.rsch"],
  "format": "部门 姓名"
}
```

等待导入任务成功，使用 `list_members` 获取 UUID。若项目还没有某个属性，先调用 `manage_properties`：

```json
{"action": "add", "names": ["岗位"]}
```

然后调用 `manage_members`，在一个操作中修改一个或多个成员：

```json
{
  "action": "update",
  "members": [
    {
      "member_id": "<list_members 返回的 ID>",
      "properties": {"姓名": "张三改名", "岗位": "值班员"},
      "note": "优先安排上午",
      "enabled": true
    }
  ]
}
```

备注不会自动成为排班硬规则；需要遵守的要求应在人工选择、脚本逻辑或 AI prompt 中明确表达。删除成员使用 `manage_members` 的 `action:"delete"` 与 `member_ids`；新增使用 `action:"add"` 与不含 ID 的 `members`。

### 5.2 编辑个人繁忙时间

先读取 `get_member_schedule`。修改其完整日期/时间列表及忙闲数据，再调用 `edit_member_schedule`：

```json
{
  "member_id": "<成员ID>",
  "dates": ["2026-11-20", "2026-11-21"],
  "time_ranges": ["08:00-10:00", "14:00-16:00"],
  "cells": [
    {"date": "2026-11-20", "time_range": "08:00-10:00", "busy": true}
  ]
}
```

这是整体替换：例中只标了一格繁忙，其余三格为空闲；未保留的原日期、时间行及繁忙数据都会删除。增删日期/时间段通过修改完整数组实现，空数组可清空个人表。最多 1000 个日期、100 个时间段和 10000 格，重复位置会拒绝。

### 5.3 配置排班和安排指定人员

`configure_schedule`：

```json
{
  "mode": "date",
  "columns": ["2026-11-20", "2026-11-21"],
  "time_ranges": ["08:00-10:00", "14:00-16:00"],
  "search_range": {
    "enabled": true,
    "start": "2026-11-20",
    "end": "2026-11-21",
    "tolerance_weeks": 0
  }
}
```

等待计算完成，调用 `query_availability` 获取具体格的候选 UUID，再调用 `assign_members`：

```json
{
  "cells": [
    {
      "target": "2026-11-21",
      "time_range": "08:00-10:00",
      "member_ids": ["<该格可用成员ID>"]
    }
  ]
}
```

这个列表替换该格原人员；追加人员时先读原列表，再提交合并后的完整列表。传空 `member_ids` 清空该格。完成后查询排班并校验冲突。

### 5.4 一键、脚本或 AI 排班草案

自动草案使用 `preview_schedule`：

```json
{
  "method": "auto",
  "target_per_cell": 2,
  "prioritize_fewer": true,
  "allow_multiple": true
}
```

脚本草案改用 `method:"script"` 并提供 `script_name`；内置 AI 草案如下：

```json
{
  "method": "ai",
  "target_per_cell": 2,
  "prompt": "尽量均衡班次，同一人不要安排在重叠时段。",
  "thinking": false
}
```

按第 6 节等待任务完成，从 **任务最终结果的 `data.draft_id`** 取 ID，调用 `get_schedule_draft` 审阅分配与属性修改。草案不会自动应用，也不表示已满足 prompt 的全部要求。

审阅后调用 `apply_schedule_draft`：

```json
{"draft_id": "<任务完成后返回的草案ID>"}
```

如只应用部分格，增加 `cell_keys`，使用读取草案时返回的键。应用后调用 `validate_schedule`，必要时使用 `undo` 的 `scope:"schedule"` 撤销。

若任务需要直接应用而无需草案，分别使用 `auto_schedule`、`run_schedule_script` 或 `ai_schedule`，不传 `method`；默认 `preview:false`。这三个工具也接受 `preview:true`。

### 5.5 保存和导出

首次保存或另存为，调用 `save_project`：

```json
{"path": "D:/值班数据/十一月值班.rroll"}
```

已有当前文件时，`save_project` 的业务 payload 可为 `{}`，仍须补齐公共字段并传入 `request_json`。需要覆盖显式指定的已有文件时增加 `overwrite:true`。

`export_excel`：

```json
{"path": "D:/值班数据/值班及成员.xlsx", "scope": "both"}
```

`export_rmem` / `import_rmem` 分别使用：

```json
{"path": "D:/值班数据/成员备份.rmem"}
```

```json
{"path": "D:/值班数据/成员备份.rmem", "mode": "merge"}
```

`export_rsch` 使用：

```json
{"member_id": "<成员ID>", "path": "D:/值班数据/张三.rsch"}
```

每次文件任务都要等待最终成功，再向用户报告实际返回的输出路径。不要在仅得到 `job_id` 时声称已保存或导出。

### 5.6 新建和修改脚本

使用 `write_script` 创建脚本：

```json
{
  "name": "agent_schedule.py",
  "code": "def generate_schedule(api, target_per_cell):\n    return {key: ids[:target_per_cell] for key, ids in api.get_task_candidates().items()}\n"
}
```

等待写入成功后，可以读取确认，再用 `preview_schedule` 的 `method:"script"`、`script_name:"agent_schedule.py"`、`target_per_cell` 试运行。

修改已有脚本时，先 `read_script`，再 `write_script`，同时提供新 `code`、`overwrite:true` 和 `expected_sha256`。删除使用 `delete_script`，提供 `name` 与最新 `expected_sha256`。文件内容已变化时先重新读取，不强行覆盖。

## 6. 后台任务与取消

`open_project`、导入、算法/脚本/AI 排班、预览、导出、保存和脚本写入会返回 `data.job_id`。初始 `ok:true` 只表示任务已接受。

调用 `get_job_status`，直接参数如下；不使用 `request_json`：

```json
{"job_id": "<初始响应的 data.job_id>"}
```

可以约每 200–500 ms 轮询一次，根据耗时调整频率。检查两层状态：外层 `ok` 表示状态查询本身是否成功；内层 `data.state` 和 `data.result` 表示任务结果。例如：

```json
{
  "ok": true,
  "data": {
    "job_id": "<job_id>",
    "operation": "auto_schedule",
    "state": "completed",
    "result": {
      "ok": true,
      "project_id": "<project_id>",
      "revision": "123",
      "data": {"draft_id": "<draft_id>", "cell_count": 2}
    }
  }
}
```

| `data.state` | 处理方式 |
| --- | --- |
| `running` | 继续等待；不要重复启动同一个业务操作 |
| `completed` | 检查 `data.result.ok` 并读取 `data.result.data` |
| `failed` | 读取 `data.result.error`，按错误类型处理 |
| `cancelled` | 任务已取消，不把它描述成业务成功 |

预览任务的 `operation` 为底层算法工具名（`auto_schedule`、`run_schedule_script` 或 `ai_schedule`），不能只据此判断是否已经应用；应检查最终结果是否包含 `draft_id`。

同一 GUI 一次接受一个运行中的 MCP 后台写任务。任务运行时仍可查询状态、读取数据及取消；其他写请求可能返回 `busy`。GUI 期间仍可能被人工修改，任务完成时会再次检查版本并拒绝应用过期结果。

取消调用 `cancel_job`，直接传 `job_id`。取消不能撤销已经完成的任务；返回 completed 时按实际完成状态处理。关闭 GUI 会取消未完成任务；辅助程序退出或单次 IPC 断开后，已接受的任务仍可能继续运行。任务及草案记录只有限保留，不跨 GUI 重启持久化。

## 7. 常见错误与恢复

| 错误码 | 处理方式 |
| --- | --- |
| `unavailable` / `disconnected` | 确认 GUI 已运行且本地业务服务就绪，连接失败时检查启动日志；恢复连接后重新读取上下文，未知写入结果先核查 |
| `no_project` | 按用户目标新建或打开；可使用该结果附带的空项目 ID 和版本字段 |
| `project_loading` / `calculating` | 短暂等待，重新读取上下文及版本；不跳过计算或改读磁盘 |
| `busy` | 等待当前 MCP 任务、GUI 保存或 GUI 排班完成，必要时查询/取消已知任务 |
| `project_changed` | 项目会话已切换；停止针对旧项目的后续写入，重新定位项目和成员 |
| `revision_conflict` | 重新读取相关数据，重新计算编辑意图；新请求用新 ID；过期草案重新生成 |
| `request_id_conflict` | 同一 ID 被用于不同请求；新操作使用新 ID，不修改重试中的原请求 |
| `timeout` / `invalid_response` | 核查是否已执行或已创建任务；不能直接判定写入失败，重试须保留原请求 ID |
| `invalid_arguments` / `invalid_path` | 对照工具定义检查字段、类型、日期、时间段、绝对路径及扩展名，修正后再提交 |
| `unsaved_changes` | 根据授权先保存或明确丢弃；不要静默丢弃人工修改 |
| `member_not_found` / `member_unavailable` | 重新读取 UUID 和空闲候选，检查成员是否删除、禁用或繁忙 |
| `mode_mismatch` / `outside_search_range` / `invalid_project_range` | 核对当前模式和项目搜索范围，需要调整时使用对应配置工具 |
| `import_validation_failed` / `invalid_file` | 检查格式和导入行错误，修正来源或成员匹配；校验失败的批次不会导入 |
| `path_required` / `file_exists` / `file_changed` | 提供输出路径、按授权覆盖，或重新读取脚本哈希；不要未经授权覆盖 |
| `write_failed` / `publish_failed` / `nothing_to_export` | 检查错误信息、磁盘权限、文件占用、脚本语法或空表情况；未成功前不报告已输出 |
| `script_not_found` / `script_unavailable` | 检查脚本列表及 Python 引擎是否可用，不改传任意路径绕过限制 |
| `script_failed` / `invalid_script_result` / `invalid_schedule_result` | 查看运行错误，修正脚本或结果后重新生成；不要直接应用未通过校验的数据 |
| `feature_disabled` / `ai_disabled` / `ai_not_configured` | 分别检查 AI 构建、界面开关和界面密钥配置；其他 MCP 功能可继续使用 |
| `ai_failed` / `job_failed` | 读取任务中的具体错误，修正后重新提交，不认为失败结果已应用 |
| `job_not_found` / `draft_not_found` | 记录可能已过期或 GUI 已重启；重新读取项目状态，必要时重新生成 |
| `nothing_to_undo` | 查看 `data.history`，检查撤销/重做范围与可用状态 |
| `request_too_large` / `response_too_large` | 当前业务 IPC 请求上限 256 KiB、响应上限 4 MiB；分批操作或缩小查询范围 |
| `launch_failed` | 检查辅助程序同目录是否有完整且可启动的 `RabbitRoll.exe` |
| `cancelled` | 保留取消状态，除非用户要求重新执行，否则不自动重启任务 |

