// vb6rtl_format_numeric_pre.inc - vb6_Format 函数体片段（位于 if (isNumericFormat || !isString) 块内）：数值校验、千分位缩放与占位符计数
// 由 src/rtl/core/vb6rtl/vb6rtl_format.c 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 294~398 行，逐行未改。

    // === Step 7: Apply numeric format ===
    if (isNumericFormat || !isString) {
        // If not a numeric format but expression is numeric, fall through to default
        if (!isNumericFormat && isNumeric) {
            free(fmtStr);
            /* Fix 117: VB6 默认数值格式 (Single 7 位有效数字, 最短往返) */
            return vb6_NumToBSTRDefault(numVal, expr.vt == vb6_vtSingle);
        }
        if (!isNumeric) {
            free(fmtStr);
            return vb6_BSTR_FromStr(L"");
        }

        double val = numVal;

        // Parse format string structure
        int hasPercent = 0;
        int hasComma1000 = 0;  // comma used as divide-by-1000
        int dotPos = -1;       // position of decimal point in format
        int beforeDot0 = 0, beforeDotHash = 0;  // digit placeholders before dot
        int afterDot0 = 0, afterDotHash = 0;    // digit placeholders after dot
        int ePos = -1;         // position of E/e in format
        int ePlusType = 0;     // 0=none, 1=E+, 2=E-, 3=e+, 4=e-
        int eDigits = 0;       // digits in exponent

        // Pre-scan: check for % and trailing comma (divide-by-1000)
        for (int i = 0; i < secLen; i++) {
            if (sec[i] == '"') { while (++i < secLen && sec[i] != '"'); continue; }
            if (sec[i] == '\\') { i++; continue; }
            if (sec[i] == '%') hasPercent = 1;
            if (sec[i] == 'E' || sec[i] == 'e') {
                if (i + 1 < secLen && (sec[i+1] == '+' || sec[i+1] == '-')) {
                    ePos = i;
                    ePlusType = (sec[i] == 'E') ? ((sec[i+1] == '+') ? 1 : 2) :
                                                 ((sec[i+1] == '+') ? 3 : 4);
                    // Count exponent digits
                    int j = i + 2;
                    while (j < secLen && (sec[j] == '0' || sec[j] == '#')) {
                        eDigits++;
                        j++;
                    }
                }
            }
        }

        // Check for comma as scaling (comma at end or adjacent to dot)
        // In VB6, a comma immediately before a period or at the end divides by 1000
        for (int i = 0; i < secLen; i++) {
            if (sec[i] == '"') { while (++i < secLen && sec[i] != '"'); continue; }
            if (sec[i] == '\\') { i++; continue; }
            if (sec[i] == ',') {
                // Check if it's a scaling comma (near end or near dot)
                // In VB6, commas between digit placeholders are thousand separators,
                // but a trailing comma or comma next to dot is a scaling operator
                int nextIsDot = (i + 1 < secLen && sec[i+1] == '.');
                int isTrailing = 1;
                for (int j = i + 1; j < secLen; j++) {
                    if (sec[j] == '0' || sec[j] == '#') { isTrailing = 0; break; }
                }
                if (nextIsDot || isTrailing) {
                    hasComma1000 = 1;
                }
            }
        }

        // Apply scaling
        double scaleVal = val;
        if (hasPercent) scaleVal *= 100.0;
        if (hasComma1000) scaleVal /= 1000.0;

        // Parse integer/fractional placeholder counts
        {
            int pastDot = 0;
            int pastE = 0;
            int iq = 0;
            for (int i = 0; i < secLen; i++) {
                char c = sec[i];
                if (c == '"') { iq = !iq; continue; }
                if (iq) continue;
                if (c == '\\') { i++; continue; }
                if (c == 'E' || c == 'e') {
                    if (i + 1 < secLen && (sec[i+1] == '+' || sec[i+1] == '-')) {
                        pastE = 1; i++; continue;
                    }
                }
                if (pastE) {
                    if (c == '0') eDigits++;
                    continue;
                }
                if (c == '.') { pastDot = 1; dotPos = i; continue; }
                if (c == ',') continue;  // thousand separator or scaling
                if (c == '%') continue;
                if (c == '0') {
                    if (pastDot) afterDot0++; else beforeDot0++;
                } else if (c == '#') {
                    if (pastDot) afterDotHash++; else beforeDotHash++;
                }
            }
        }

        int totalBeforeDot = beforeDot0 + beforeDotHash;
        int totalAfterDot = afterDot0 + afterDotHash;
        int decimalPlaces = afterDot0 + afterDotHash;

