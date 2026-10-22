#!/usr/bin/env python3
"""Compares the `external fun` declarations of OnAirNative.kt with the JNI functions of native/src/jni_onair.cpp (names, parameter and return types)."""
import re, sys, os
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
kt = open(f'{root}/app/src/main/java/io/github/adswill/onair/OnAirNative.kt').read()
cc = open(f'{root}/native/src/jni_onair.cpp').read()
K = {'Int': 'jint', 'Long': 'jlong', 'Double': 'jdouble', 'Float': 'jfloat', 'Boolean': 'jboolean', 'String': 'jstring', 'FloatArray': 'jfloatArray',
     'DoubleArray': 'jdoubleArray', 'Unit': 'void'}
kts = {}
for m in re.finditer(r'external fun (\w+)\((.*?)\)(?:\s*:\s*(\w+))?\s*$', kt, re.M | re.S):
    name, params, ret = m.group(1), m.group(2), m.group(3) or 'Unit'
    ps = [K[p.split(':')[1].strip()] for p in params.replace('\n', ' ').split(',') if p.strip()]
    kts[name] = (K[ret], ps)
ccs = {}
for m in re.finditer(r'FN\((\w+), (\w+)\)\(JNIEnv\*(?: e)?, jobject(.*?)\)\s*\{', cc, re.S):
    ret, name, params = m.groups()
    ps = [p.strip().split()[0] for p in params.replace('\n', ' ').split(',') if p.strip()]
    ccs[name] = (ret, ps)
bad = 0
for n in sorted(set(kts) | set(ccs)):
    if n not in kts: print('only in C++:', n); bad += 1
    elif n not in ccs: print('only in Kotlin:', n); bad += 1
    elif kts[n] != ccs[n]: print('MISMATCH', n, kts[n], ccs[n]); bad += 1
print('checked', len(kts), 'Kotlin and', len(ccs), 'C++ functions,', bad, 'problems')
sys.exit(1 if bad else 0)
