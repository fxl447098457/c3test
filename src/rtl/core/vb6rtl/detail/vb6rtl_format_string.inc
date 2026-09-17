// vb6rtl_format_string.inc - vb6_Format 函数体片段：格式类型判定与字符串格式应用
// 由 src/rtl/core/vb6rtl/vb6rtl_format.c 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 198~293 行，逐行未改。

    // === Step 5: Detect format type (numeric vs string) ===
    // Numeric format chars: 0 # . , % E- E+ e- e+
    // String format chars: @ & < > !
    int isNumericFormat = 0;
    int isStringFormat = 0;
    {
        int iq = 0;
        for (int i = 0; i < secLen; i++) {
            char c = sec[i];
            if (c == '"') { iq = !iq; continue; }
            if (iq) continue;
            if (c == '\\') { i++; continue; }
            if (c == '0' || c == '#' || c == '.' || c == '%' ||
                c == 'E' || c == 'e') {
                isNumericFormat = 1; break;
            }
            if (c == '@' || c == '&' || c == '<' || c == '>' || c == '!') {
                isStringFormat = 1; break;
            }
        }
    }

    // === Step 6: Apply string format ===
    if (isStringFormat && (isString || isNull)) {
        const wchar_t* src = (isString && strVal) ? strVal : L"";
        int32_t srcLen = (int32_t)wcslen(src);

        // Detect < and >
        int forceLower = 0, forceUpper = 0, leftToRight = 0;
        for (int i = 0; i < secLen; i++) {
            if (sec[i] == '"') { while (++i < secLen && sec[i] != '"'); continue; }
            if (sec[i] == '<') forceLower = 1;
            if (sec[i] == '>') forceUpper = 1;
            if (sec[i] == '!') leftToRight = 1;
        }

        // Count @ and & placeholders
        int numPlaceholders = 0;
        for (int i = 0; i < secLen; i++) {
            if (sec[i] == '"') { while (++i < secLen && sec[i] != '"'); continue; }
            if (sec[i] == '@' || sec[i] == '&') numPlaceholders++;
        }

        // Build output: fill placeholders from right-to-left (default) or left-to-right (!)
        wchar_t outBuf[512];
        int outLen = 0;
        int charIdx = leftToRight ? 0 : srcLen - 1;
        int charDir = leftToRight ? 1 : -1;
        int charsUsed = 0;

        // Iterate format string
        int iq = 0;
        for (int i = leftToRight ? 0 : secLen - 1;
             leftToRight ? (i < secLen) : (i >= 0);
             leftToRight ? i++ : i--) {
            char c = sec[i];
            if (c == '"') { iq = !iq; continue; }
            if (iq) {
                outBuf[outLen++] = (wchar_t)(unsigned char)c;
                continue;
            }
            if (c == '\\') {
                if (++i < secLen && i >= 0) outBuf[outLen++] = (wchar_t)(unsigned char)sec[i];
                continue;
            }
            if (c == '@') {
                if (charsUsed < srcLen) {
                    outBuf[outLen++] = src[charIdx];
                    charIdx += charDir;
                    charsUsed++;
                } else {
                    outBuf[outLen++] = L' ';  // @ pads with space
                }
            } else if (c == '&') {
                if (charsUsed < srcLen) {
                    outBuf[outLen++] = src[charIdx];
                    charIdx += charDir;
                    charsUsed++;
                }
                // & does not pad
            } else if (c == '<' || c == '>' || c == '!') {
                // control chars, skip
            } else {
                outBuf[outLen++] = (wchar_t)(unsigned char)c;
            }
        }
        outBuf[outLen] = L'\0';

        // Apply case transforms
        if (forceLower) { for (int i = 0; i < outLen; i++) outBuf[i] = (wchar_t)towlower(outBuf[i]); }
        if (forceUpper) { for (int i = 0; i < outLen; i++) outBuf[i] = (wchar_t)towupper(outBuf[i]); }

        free(fmtStr);
        return vb6_BSTR_FromStr(outBuf);
    }

