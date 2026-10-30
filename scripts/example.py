import random

def generate_schedule(api, target_per_cell):    
    data = api.get_task_candidates()  

    cnt = api.get_member_duty_counts()
    res = {}
    used = set()

    # 按候选人数量从少到多处理，避免后期人手不够
    ordered = sorted(data.keys(), key=lambda k: len(data[k]))

    for k in ordered:
        cands = data[k]
        if not cands:
            res[k] = []
            continue

        random.shuffle(cands) 
        #按当前值班次数升序，次数少的优先
        cands.sort(key=lambda uid: cnt.get(str(uid), 0))

        picked = []
        for u in cands:
            if len(picked) >= target_per_cell:
                break
            
            ustr = str(u)
            picked.append(ustr)
            cnt[ustr] = cnt.get(ustr, 0) + 1

        res[k] = picked

    return res
