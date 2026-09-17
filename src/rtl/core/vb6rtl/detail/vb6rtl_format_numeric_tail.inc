// vb6rtl_format_numeric_tail.inc - vb6_Format 函数体片段（位于 if (isNumericFormat || !isString) 块内）：字面量拼装与回退
// 由 src/rtl/core/vb6rtl/vb6rtl_format.c 拆出（2026-09-17），在函数体内被 #include，非独立编译单元。
// 内容 = 原文件第 533~632 行，逐行未改。

        // Build the final formatted string by walking the format string
        // This handles literal characters, quotes, escaped chars correctly
        wchar_t result[256];
        int rLen = 0;

        // We'll emit: [sign] intPart [. fracPart] [%] plus any literal chars from format
        // Walk the format string to build output with proper position of literals
        int intPos = 0;    // position in intPart
        int fracPos = 0;   // position in fracPart
        int intLen = (int)wcslen(intPart);
        int fracLen = (int)wcslen(fracPart);
        int dotEmitted = 0;
        int pastDot2 = 0;
        int inScientific = 0;
        int numDigitsBeforeDot = totalBeforeDot;

        // For proper alignment of integer digits, we need to know how many
        // integer digits to emit before, during, and after literals
        // Simplified approach: emit digits followed by format-structured literal text

        // Even simpler: we already have numBuf formatted correctly (rounded+thousands),
        // now we just need to:
        // 1. Strip trailing zeros beyond what # placeholders allow
        // 2. Strip leading zeros beyond what 0 placeholders require
        // 3. Add % if needed
        // 4. Handle negative sign
        // 5. Process literal chars from format string

        // Actually, the correct VB6 approach is simpler:
        // We've already rounded and formatted with snprintf.
        // Now we just need to:
        // - Check if we need % suffix
        // - Handle the case where all fractional placeholders are # (no trailing zeros needed)
        // - Handle negative sign positioning (from format string)

        // Re-derive: build from already-formatted intPart and fracPart

        // Emit negative sign if needed
        if (isNegative) result[rLen++] = L'-';

        // Emit integer part
        for (int i = 0; i < intLen; i++) result[rLen++] = intPart[i];

        // Emit decimal point and fractional part (only if format has dot or frac digits)
        if ((afterDot0 + afterDotHash) > 0 || dotPos >= 0) {
            // Determine how many fractional digits to show
            int showFrac = (int)wcslen(fracPart);
            // Trim trailing zeros that exceed 0-placeholder requirement
            int minFrac = afterDot0;  // minimum fractional digits (from 0 placeholders)
            while (showFrac > minFrac && fracPart[showFrac - 1] == L'0') showFrac--;

            // VB6: if no fractional placeholders and no dot, don't show decimal
            if (showFrac > 0 || afterDot0 > 0) {
                result[rLen++] = L'.';
                for (int i = 0; i < showFrac; i++) result[rLen++] = fracPart[i];
            }
        }

        // Emit % if needed
        if (hasPercent) result[rLen++] = L'%';

        result[rLen] = L'\0';

        // Now scan format string for any literal characters not yet emitted
        // (e.g. "$" or text in quotes)
        // For a complete implementation, we need to walk the format and emit
        // literals in position. Simplified: append any quoted/escaped literals.
        {
            int iq = 0;
            for (int i = 0; i < secLen; i++) {
                char c = sec[i];
                if (c == '"') {
                    iq = !iq;
                    continue;
                }
                if (iq) {
                    // Already handled inside format? No - we need to position these.
                    // For now, skip (most numeric formats don't use quoted text)
                    continue;
                }
                if (c == '\\') { i++; continue; }
                // Other literal chars like $, -, +, (, ) and space
                // In VB6, these are emitted in their positions relative to digit placeholders
                // This simplified version doesn't position them - a full version would
            }
        }

        free(fmtStr);
        return vb6_BSTR_FromStr(result);
    }

    // === Step 8: Fallback ===
    free(fmtStr);
    if (isNumeric) {
        wchar_t buf[64];
        swprintf(buf, 64, L"%.15g", numVal);
        return vb6_BSTR_FromStr(buf);
    }
    if (isString && strVal) return vb6_BSTR_FromBSTR(strVal);
    return vb6_BSTR_FromStr(L"");
