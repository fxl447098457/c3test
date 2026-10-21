# owned_temp_census_164.py — 账 #463 §B164: 「现造带权临时量、无人持有」在**语料**上的清点
#
# 问句（研究用，**零行为改动**）：
#   语料里有多少处「一条语句里现造的带权临时量，传进去之后没人持有」？
#   判据（权威全部来自 scripts/owned_temp_keeps_164.txt，那张表由
#   scripts/check_owned_temp_keeps.ps1 从 RTL **推导** ⇒ 哨兵双向对账 ⇒ 漂不了）：
#     ① mint 侧：出口在 [own] 节（新造且归我们所有）；不在 [borrow]（借来）/[alias]（别名）
#        —— 档号**只由出口的返回档定**（BSTR / VARIANT），**不是**消费者的 VB 形参档
#           （#461 的病根就是拿了形参档 ⇒ 一枚 double 被声明成 BSTR ⇒ cl C2440）
#     ② consumer 侧：keeps_pointer == '0'（不带走指针）· 不在 closed 族 ·
#        **不在消费轴上**（消费者自己放掉这一格 ⇒ 再放就是双放 ⇒ 堆损坏）
#     ③ 站点的形状 = 「整枚实参 = 一个 mint 调用」—— 站点取**发码文本**（--emit-c），
#        所以「消费者第几格」取的是**发码里那一格的位号**（不是 VB 实参下标）
#
# 跑法:
#   python scripts/owned_temp_census_164.py --dump     # 只把语料发码落到缓存
#   python scripts/owned_temp_census_164.py            # 清点 + 机械证明
#   退出码: 0 = 证明都成立；1 = 有证明不成立（那是判据或表出了事）
import argparse
import collections
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

TABLE = os.path.join(ROOT, 'scripts', 'owned_temp_keeps_164.txt')
C3 = os.path.join(ROOT, '.build', 'C3.exe')
CACHE = os.path.join(ROOT, 'output', '_tmp', 'census_c')


# ---------------------------------------------------------------- 证据表
def load_table():
    own, alias, borrowed, keeps, closed, consumes = {}, set(), set(), {}, set(), {}
    section = None
    for raw in open(TABLE, encoding='utf-8'):
        line = raw.rstrip('\n')
        if not line.strip() or line.startswith('#'):
            # 节头：# --- ... owned ... / alias ... / 消费轴 ... / 借用轴 ...
            if '返回侧所有权：owned' in line:
                section = 'own'
            elif 'alias = 交出去的是' in line:
                section = 'alias'
            elif '消费轴' in line:
                section = 'consumes'
            elif '借用轴' in line:
                section = 'borrow'
            continue
        if line.startswith('['):
            tag, rest = line.split(']', 1)
            rest = rest.strip()
            if tag == '[own':
                own[rest.split()[0]] = rest.split()[1]
            elif tag == '[alias':
                alias.add(rest.split()[0])
            elif tag == '[borrow':
                borrowed.add(rest.split()[0])
            section = None
            continue
        if section == 'consumes':
            f = line.split('\t')
            if len(f) >= 3:
                consumes[(f[0], int(f[0].split('|')[1]))] = (f[1], f[2], f[3] if len(f) > 3 else '')
            continue
        if section is not None:
            continue
        f = line.split('\t')
        if len(f) < 6:
            continue
        nm, slot = f[0].split('|')
        keeps[(nm, int(slot))] = f[1]
        if f[4] == 'closed':
            closed.add(nm)
    return own, alias, borrowed, keeps, closed, consumes


# ---------------------------------------------------------------- 发码解析
IDENT = re.compile(r'[A-Za-z_][A-Za-z0-9_]*')
CALL = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)\s*\(')


def strip_code(src):
    """去掉注释与字符串字面量（保留位置，换成空格）"""
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            while i < n and src[i] != '\n':
                out[i] = ' '
                i += 1
        elif c == '/' and i + 1 < n and src[i + 1] == '*':
            out[i] = out[i + 1] = ' '
            i += 2
            while i < n and not (src[i] == '*' and i + 1 < n and src[i + 1] == '/'):
                if src[i] != '\n':
                    out[i] = ' '
                i += 1
            if i < n:
                out[i] = ' '
                i += 2
        elif c in '"\'':
            q = c
            out[i] = ' '
            i += 1
            while i < n and src[i] != q:
                if src[i] == '\\':
                    out[i] = ' '
                    i += 1
                if i < n:
                    out[i] = ' '
                    i += 1
            if i < n:
                out[i] = ' '
                i += 1
        else:
            i += 1
    return ''.join(out)


def match_paren(s, i):
    d = 0
    while i < len(s):
        if s[i] == '(':
            d += 1
        elif s[i] == ')':
            d -= 1
            if d == 0:
                return i
        i += 1
    return -1


def split_args(s, o, c):
    args, d, st = [], 0, o + 1
    for i in range(o + 1, c):
        ch = s[i]
        if ch in '([{':
            d += 1
        elif ch in ')]}':
            d -= 1
        elif ch == ',' and d == 0:
            args.append(s[st:i])
            st = i + 1
    args.append(s[st:c])
    return [a.strip() for a in args]


def corpus_inputs():
    ins = []
    for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, 'tests')):
        dirnames[:] = [d for d in dirnames if d not in ('bin', 'obj', 'output')]
        for fn in filenames:
            if fn.lower().endswith(('.bas', '.vbp')):
                ins.append(os.path.join(dirpath, fn))
    ins.sort()
    return ins


def dump_corpus(inputs):
    os.makedirs(CACHE, exist_ok=True)
    rc_bad = 0
    for path in inputs:
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        flat = re.sub(r'[\\/ \-]', '_', rel)
        dst = os.path.join(CACHE, flat + '.c')
        if os.path.exists(dst):
            continue
        try:
            p = subprocess.run([C3, path, '--emit-c'], capture_output=True, timeout=180)
        except Exception:
            rc_bad += 1
            continue
        if p.returncode != 0:
            rc_bad += 1
        with open(dst, 'wb') as f:
            f.write(p.stdout)
    return rc_bad


# ---------------------------------------------------------------- 主
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dump', action='store_true', help='只把语料发码落到缓存')
    args = ap.parse_args()

    own, alias, borrowed, keeps, closed, consumes = load_table()
    print('evidence: own=%d alias=%d borrow=%d keeps=%d closed=%d consumes=%d'
          % (len(own), len(alias), len(borrowed), len(keeps), len(closed), len(consumes)))

    inputs = corpus_inputs()
    print('corpus inputs =', len(inputs))
    rc_bad = dump_corpus(inputs)
    print('emit rc!=0 =', rc_bad, '(那一族是 *_neg* 负例夹具，本来就红)')
    if args.dump:
        return 0

    def mint_kind(callee):
        if callee in borrowed or callee in alias:
            return 0
        if callee in own:
            return 'V' if own[callee] == 'VARIANT' else 'B'
        return 0

    def verdict(consumer, slot):
        if consumer in closed:
            return 0
        if keeps.get((consumer, slot), '0') != '0':
            return 0
        if (consumer, slot) in consumes:
            return 0
        return 1

    nom = []
    shapes = collections.Counter()
    for path in sorted(os.listdir(CACHE)):
        if not path.endswith('.c'):
            continue
        code = strip_code(open(os.path.join(CACHE, path), encoding='latin-1').read())
        i = 0
        while i < len(code):
            m = IDENT.match(code, i)
            if m and m.end() < len(code) and code[m.end()] == '(':
                consumer = code[i:m.end()]
                o = m.end()
                c = match_paren(code, o)
                if c < 0:
                    i += 1
                    continue
                for slot, a in enumerate(split_args(code, o, c)):
                    mm = CALL.match(a)
                    if not mm or a.count('(') != a.count(')'):
                        continue
                    mint = mm.group(1)
                    kind = mint_kind(mint)
                    if not kind:
                        continue
                    if not verdict(consumer, slot):
                        continue
                    nom.append((path[:-2], consumer, slot, mint, kind))
                    shapes[(mint, kind, consumer, slot)] += 1
                i = m.end()
                continue
            i += 1

    print('\nnomination set =', len(nom), ' in', len({n[0] for n in nom}), 'files')
    proofs = [
        ('intersect closed', sum(1 for n in nom if n[1] in closed)),
        ('intersect borrow', sum(1 for n in nom if n[3] in borrowed)),
        ('intersect alias', sum(1 for n in nom if n[3] in alias)),
        ('intersect consumes', sum(1 for n in nom if (n[1], n[2]) in consumes)),
    ]
    bad = 0
    for name, v in proofs:
        print('  proof %-20s = %d' % (name, v))
        if v != 0:
            bad += 1
    b = sum(1 for n in nom if n[4] == 'B')
    print('  kinds: BSTR = %d  VARIANT = %d' % (b, len(nom) - b))
    print('\n--- top shapes ---')
    for (mint, kind, cons, slot), n in shapes.most_common(25):
        print('  mint=%-22s(%s) -> consumer=%-26s slot=%d n=%d' % (mint, kind, cons, slot, n))
    if bad:
        print('\nFAIL: %d proofs do not hold' % bad)
        return 1
    print('\nALL-PROOFS-HOLD')
    return 0


if __name__ == '__main__':
    sys.exit(main())
