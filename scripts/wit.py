import random

def generate_schedule(api, target_per_cell):    
    data = api.get_task_candidates()  
    members = api.get_all_members()
    
    props_cache = {}
    for uid in members:
        props_cache[uid] = api.get_member_properties(uid)

    def load_cnt(uid):
        p = props_cache.get(uid, {})
        for key in ["值班次数"]:
            if key in p:
                try:
                    return int(p[key])
                except ValueError:
                    pass
        return 0

    def dept(uid):
        p = props_cache.get(uid, {})
        for key in ["部门"]:
            if key in p:
                return p[key]
        return "未知"

    def name(uid):
        p = props_cache.get(uid, {})
        for key in ["姓名"]:
            if key in p:
                return p[key]
        return uid[:6]

    api.debug("开始排班")

    res = {}
    used = set()
    ordered = sorted(data.keys(), key=lambda k: len(data[k]))

    for k in ordered:
        cands = data[k]
        if not cands:
            res[k] = []
            api.debug(f"[!] {k} 无候选人")
            continue

        # 过滤掉本轮已安排的人
        avail = [str(u) for u in cands if str(u) not in used]
        random.shuffle(avail) 
        avail.sort(key=lambda u: load_cnt(u))

        picked = []
        pool = list(avail)
        
        while pool and len(picked) < target_per_cell:
            sel = None
            if len(picked) == 0:
                sel = pool.pop(0)
            else:
                dept0 = dept(picked[0])
                for i, u in enumerate(pool):
                    if dept(u) != dept0:
                        sel = pool.pop(i)
                        break
                if sel is None:
                    sel = pool.pop(0)

            if sel:
                picked.append(sel)
                used.add(sel)
                
                cur_cnt = load_cnt(sel)
                new_cnt = cur_cnt + 1
                api.set_member_property(sel, "值班次数", str(new_cnt))
                
                # 同步更新本地缓存
                if sel not in props_cache:
                    props_cache[sel] = {}
                props_cache[sel]["值班次数"] = str(new_cnt)

        if len(picked) < target_per_cell:
            api.debug(f"[!] {k} 人手不足")

        # 打一行日志看下安排结果
        info = ', '.join(f"{name(u)}({dept(u)})" for u in picked)
        api.debug(f"{k} -> {info}")
        res[k] = picked

    api.debug("排班完成")
    return res