/**
 * C3 Landing Page - 最新版本 / 最新提交动态
 *
 * 数据源: GitCode Open API v5 (公开只读, 无需令牌, 允许跨域)
 *   GET /repos/{owner}/{repo}/releases/latest
 *   GET /repos/{owner}/{repo}/commits?sha=main&per_page=1
 *
 * 设计原则: 接口拿不到数据只降级, 绝不破坏页面.
 *   - 页面 HTML 内已写好静态兜底链接, JS 失败时下载按钮仍然可用
 *   - 结果写入 localStorage, 10 分钟内复用, 避免频繁请求
 */

(function () {
    'use strict';

    var CFG = {
        repo: 'woeoio/c3.vb6.pro',
        branch: 'main',
        api: 'https://api.gitcode.com/api/v5/repos/',
        home: 'https://gitcode.com/woeoio/c3.vb6.pro',
        cacheKey: 'c3:site-release-meta',
        cacheTTL: 10 * 60 * 1000,
        timeout: 8000
    };

    var $ = function (id) { return document.getElementById(id); };
    var q = function (sel) { return document.querySelector(sel); };

    var els = {
        heroBtn: $('heroDownload'),
        heroLabel: q('#heroDownload .btn-label'),
        heroVersion: $('heroVersion'),
        cardBtn: $('downloadBtn'),
        cardLabel: q('#downloadBtn .btn-label'),
        cardVersion: $('downloadVersion'),
        meta: $('releaseMeta'),
        tag: $('releaseTag'),
        date: $('releaseDate'),
        commitMsg: $('commitMsg'),
        commitMeta: $('commitMeta')
    };

    // ---------- 工具 ----------

    function shortDate(iso) {
        var d = new Date(iso);
        if (isNaN(d.getTime())) return '';
        var p = function (n) { return (n < 10 ? '0' : '') + n; };
        return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate());
    }

    function agoLabel(iso) {
        var t = new Date(iso).getTime();
        if (isNaN(t)) return '';
        var diff = Date.now() - t;
        if (diff < 0) return '刚刚';
        var MIN = 60000, HOUR = 3600000, DAY = 86400000;
        if (diff < MIN) return '刚刚';
        if (diff < HOUR) return Math.floor(diff / MIN) + ' 分钟前';
        if (diff < DAY) return Math.floor(diff / HOUR) + ' 小时前';
        if (diff < 30 * DAY) return Math.floor(diff / DAY) + ' 天前';
        if (diff < 365 * DAY) return Math.floor(diff / (30 * DAY)) + ' 个月前';
        return Math.floor(diff / (365 * DAY)) + ' 年前';
    }

    /* 合并提交的首行通常只有 MR 标题, 退回到更有信息量的 Description 行 */
    function commitTitle(message) {
        if (!message) return '';
        var lines = message.split('\n');
        var head = (lines[0] || '').trim();
        if (/see merge request/i.test(message)) {
            for (var i = 0; i < lines.length; i++) {
                if (/^description:/i.test(lines[i].trim())) {
                    return lines[i].trim().replace(/^description:/i, '').trim();
                }
            }
        }
        return head;
    }

    // ---------- 数据获取 ----------

    function getJSON(url) {
        var timer, ctl;
        var opts = { headers: { Accept: 'application/json' }, cache: 'no-store' };
        if (typeof AbortController === 'function') {
            ctl = new AbortController();
            opts.signal = ctl.signal;
            timer = setTimeout(function () { ctl.abort(); }, CFG.timeout);
        }
        return fetch(url, opts).then(function (res) {
            if (!res.ok) throw new Error('HTTP ' + res.status);
            return res.json();
        }).then(function (data) {
            if (timer) clearTimeout(timer);
            return data;
        }, function (err) {
            if (timer) clearTimeout(timer);
            throw err;
        });
    }

    function readCache() {
        try {
            var raw = localStorage.getItem(CFG.cacheKey);
            if (!raw) return null;
            var box = JSON.parse(raw);
            if (!box || !box.payload || Date.now() - box.ts > CFG.cacheTTL) return null;
            return box.payload;
        } catch (e) {
            return null;
        }
    }

    function writeCache(payload) {
        try {
            localStorage.setItem(CFG.cacheKey, JSON.stringify({ ts: Date.now(), payload: payload }));
        } catch (e) { /* 隐私模式忽略 */ }
    }

    function normalizeRelease(rel) {
        if (!rel) return null;
        var assets = Array.isArray(rel.assets) ? rel.assets : [];
        var attaches = assets.filter(function (a) { return a && a.type === 'attach'; });
        var pick = null;
        for (var i = 0; i < attaches.length; i++) {
            if (/\.zip$/i.test(attaches[i].name || '')) { pick = attaches[i]; break; }
        }
        pick = pick || attaches[0] || null;
        var tag = rel.tag_name || rel.name || '';
        return {
            tag: tag,
            file: pick ? pick.name : '',
            url: pick ? pick.browser_download_url : '',
            publishedAt: rel.created_at || '',
            releasesUrl: CFG.home + '/releases'
        };
    }

    function normalizeCommit(c) {
        if (!c || !c.commit) return null;
        var who = c.commit.committer || c.commit.author || {};
        var name = (c.author && (c.author.name || c.author.login)) || who.name || '';
        return {
            sha: (c.sha || '').slice(0, 7),
            url: c.html_url || '',
            title: commitTitle(c.commit.message),
            author: name,
            date: who.date || ''
        };
    }

    function fetchMeta() {
        var relUrl = CFG.api + CFG.repo + '/releases/latest';
        var commitUrl = CFG.api + CFG.repo + '/commits?sha=' + encodeURIComponent(CFG.branch) + '&per_page=1';
        return Promise.all([
            getJSON(relUrl).catch(function () { return null; }),
            getJSON(commitUrl).catch(function () { return null; })
        ]).then(function (pair) {
            var release = normalizeRelease(pair[0]);
            var commit = normalizeCommit(Array.isArray(pair[1]) ? pair[1][0] : null);
            if (!release && !commit) throw new Error('release 与 commit 接口均不可用');
            return { release: release, commit: commit };
        });
    }

    // ---------- 渲染 ----------

    function applyDownload(btn, label, url, text) {
        if (!btn || !url) return;
        btn.setAttribute('href', url);
        btn.removeAttribute('target');   // 直连 CDN 触发下载, 不留空白标签页
        if (label) label.textContent = text;
    }

    function render(meta) {
        var rel = meta && meta.release;
        var cm = meta && meta.commit;

        if (rel && rel.url) {
            applyDownload(els.heroBtn, els.heroLabel, rel.url, '下载 C3 ' + rel.tag);
            applyDownload(els.cardBtn, els.cardLabel, rel.url, '下载 C3 ' + rel.tag);
        }

        if (rel && rel.tag) {
            if (els.heroVersion) els.heroVersion.textContent = rel.tag;
            if (els.cardVersion) {
                var parts = [rel.tag, 'Windows x64'];
                if (rel.publishedAt) parts.push('发布于 ' + shortDate(rel.publishedAt));
                if (rel.file) parts.push(rel.file);
                els.cardVersion.textContent = parts.join(' · ');
                els.cardVersion.title = rel.file || '';
            }
        }

        if (els.meta) els.meta.classList.remove('is-loading');

        if (els.tag && rel && rel.tag) {
            els.tag.textContent = rel.tag;
            els.tag.href = rel.releasesUrl;
        }
        if (els.date && rel && rel.publishedAt) {
            els.date.textContent = '发布于 ' + shortDate(rel.publishedAt) + ' · ' + agoLabel(rel.publishedAt);
        }

        if (cm && cm.title) {
            if (els.commitMsg) {
                els.commitMsg.textContent = cm.title;
                if (cm.url) els.commitMsg.href = cm.url;
            }
            if (els.commitMeta) {
                var bits = [];
                if (cm.sha) bits.push(cm.sha);
                if (cm.author) bits.push(cm.author);
                if (cm.date) bits.push(agoLabel(cm.date));
                els.commitMeta.textContent = bits.join(' · ');
                els.commitMeta.title = cm.date ? shortDate(cm.date) : '';
            }
        }
    }

    function fail(err) {
        if (els.meta) {
            els.meta.classList.remove('is-loading');
            els.meta.classList.add('is-failed');
            if (els.tag) els.tag.textContent = '查看发布页';
            if (els.date) els.date.textContent = '版本信息暂未取到, 可直接下载上方版本';
        }
        if (window.console && console.warn) console.warn('[C3] 获取最新版本信息失败:', err && err.message);
    }

    // ---------- 启动 ----------

    function boot() {
        if (!els.meta && !els.heroBtn && !els.cardBtn) return;

        var cached = readCache();
        if (cached) render(cached);
        else if (els.meta) els.meta.classList.add('is-loading');

        fetchMeta().then(function (meta) {
            writeCache(meta);
            render(meta);
        }, function (err) {
            if (!cached) fail(err);
        });
    }

    if (typeof fetch !== 'function') return;
    boot();

})();
