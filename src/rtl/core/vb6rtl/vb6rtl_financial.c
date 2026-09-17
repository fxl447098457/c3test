// vb6rtl_financial.c - VB6 运行时库: 财务家族：SLN/SYD/DDB/FV/PV/Pmt/IPmt/PPmt/RATE/NPV
// 2026-09-17 从 src/rtl/core/vb6rtl/vb6rtl.c 按家族拆出（纯搬移，逐行未改）:
//   原第 4431~4594 行

#include "vb6rtl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <wchar.h>
#include <wctype.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <oleauto.h>
#include <olectl.h>
#include <windows.h>
#endif

// P24-08: MessageBoxW (user32) + GetConsoleWindow (kernel32)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

// ============================================================
// P18-E: Financial Functions
// ============================================================

// SLN - Straight Line Depreciation
double vb6_SLN(double cost, double salvage, double life) {
    if (life == 0.0) return 0.0;
    return (cost - salvage) / life;
}

// SYD - Sum of Years' Digits Depreciation
double vb6_SYD(double cost, double salvage, double life, double period) {
    if (life == 0.0) return 0.0;
    return (cost - salvage) * (life - period + 1.0) * 2.0 / (life * (life + 1.0));
}

// DDB - Double Declining Balance Depreciation
double vb6_DDB(double cost, double salvage, double life, double period, double factor) {
    if (life == 0.0) return 0.0;
    double bookValue = cost;
    double depreciation = 0.0;
    int pmax = (int)period;
    for (int p = 1; p <= pmax; p++) {
        depreciation = bookValue * factor / life;
        if (bookValue - depreciation < salvage) {
            depreciation = bookValue - salvage;
        }
        if (p == pmax) break;
        bookValue -= depreciation;
        if (bookValue <= salvage) {
            if (p == pmax) break;
            depreciation = 0.0;
            break;
        }
    }
    return depreciation;
}

// FV - Future Value of an annuity
double vb6_FV(double rate, double nper, double pmt, double pv, int32_t type) {
    if (rate == 0.0) {
        return -(pv + pmt * nper);
    }
    double factor = pow(1.0 + rate, nper);
    return -(pv * factor + pmt * (factor - 1.0) / rate * (1.0 + rate * (double)type));
}

// PV - Present Value of an annuity
double vb6_PV(double rate, double nper, double pmt, double fv, int32_t type) {
    if (rate == 0.0) {
        return -(fv + pmt * nper);
    }
    double factor = pow(1.0 + rate, nper);
    return -(fv / factor + pmt * (1.0 - 1.0 / factor) / rate * (1.0 + rate * (double)type));
}

// Pmt - Periodic Payment
double vb6_Pmt(double rate, double nper, double pv, double fv, int32_t type) {
    if (rate == 0.0) {
        if (nper == 0.0) return 0.0;
        return -(pv + fv) / nper;
    }
    double factor = pow(1.0 + rate, nper);
    return -(pv * factor + fv) * rate / ((factor - 1.0) * (1.0 + rate * (double)type));
}

// IPmt - Interest Payment for a specific period
double vb6_IPmt(double rate, double per, double nper, double pv, double fv, int32_t type) {
    (void)nper; (void)fv;
    double pmt = vb6_Pmt(rate, nper, pv, fv, type);
    double n = per - 1.0;
    if (type == 1) n -= 1.0;
    if (n < 0.0) return 0.0;

    double balance;
    if (rate == 0.0) {
        balance = pv + pmt * n;
    } else {
        double factor = pow(1.0 + rate, n);
        balance = pv * factor + pmt * (factor - 1.0) / rate;
    }
    return -balance * rate;
}

// PPmt - Principal Payment for a specific period
double vb6_PPmt(double rate, double per, double nper, double pv, double fv, int32_t type) {
    double pmt = vb6_Pmt(rate, nper, pv, fv, type);
    double ipmt = vb6_IPmt(rate, per, nper, pv, fv, type);
    return pmt - ipmt;
}

// RATE - Interest rate per period (Newton's method iteration)
double vb6_RATE(double nper, double pmt, double pv, double fv, int32_t type, double guess) {
    double rate = guess;
    if (rate == 0.0) rate = 0.1;

    for (int iter = 0; iter < 100; iter++) {
        double factor = pow(1.0 + rate, nper);
        double f;
        if (rate == 0.0) {
            f = pv + pmt * nper * (1.0 + rate * (double)type) + fv;
        } else {
            f = pv * factor + pmt * (1.0 + rate * (double)type) * (factor - 1.0) / rate + fv;
        }

        double delta = rate * 0.0001;
        if (delta < 1e-10) delta = 1e-10;
        double rate2 = rate + delta;
        double factor2 = pow(1.0 + rate2, nper);
        double f2;
        if (rate2 == 0.0) {
            f2 = pv + pmt * nper * (1.0 + rate2 * (double)type) + fv;
        } else {
            f2 = pv * factor2 + pmt * (1.0 + rate2 * (double)type) * (factor2 - 1.0) / rate2 + fv;
        }
        double fp = (f2 - f) / delta;

        if (fabs(fp) < 1e-15) break;

        double newRate = rate - f / fp;
        if (fabs(newRate - rate) < 1e-10) {
            rate = newRate;
            break;
        }
        rate = newRate;
    }
    return rate;
}

// NPV - Net Present Value
double vb6_NPV(double rate, struct vb6_SafeArray1D* values) {
    if (!values || !values->data || values->count <= 0) return 0.0;
    double npv = 0.0;

    for (int32_t i = 0; i < values->count; i++) {
        double val = 0.0;
        switch (values->elemType) {
            case 6: /* vb6_sa_double */
                val = ((double*)values->data)[i];
                break;
            case 5: /* vb6_sa_single */
                val = (double)((float*)values->data)[i];
                break;
            case 4: /* vb6_sa_long */
                val = (double)((int32_t*)values->data)[i];
                break;
            case 3: /* vb6_sa_int */
                val = (double)((int16_t*)values->data)[i];
                break;
            case 2: /* vb6_sa_byte */
                val = (double)((uint8_t*)values->data)[i];
                break;
            case 8: /* vb6_sa_variant */
                val = vb6_VariantToDouble(((vb6_VARIANT*)values->data)[i]);
                break;
            default:
                val = 0.0;
                break;
        }
        npv += val / pow(1.0 + rate, (double)(i + 1));
    }
    return npv;
}

