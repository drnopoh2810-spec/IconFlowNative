# IconFlow Native Picker — دليل البناء والتثبيت

إضافة Adobe After Effects الأصلية (`.aex`) المكتوبة بـ C++، تُنفّذ عزل الخلفية وإعادة التلوين والتوهج مع قطّارة لون حقيقية داخل لوحة التكوين (Custom Comp UI).

---

## المتطلبات

| المكوّن | الإصدار المطلوب |
|---------|----------------|
| Windows | 10 أو 11 (x64) |
| Adobe After Effects | 2020 (17.x) أو أحدث؛ مُختبر مع 2024 (24.x) |
| Adobe After Effects SDK | 2022 أو 2024 |
| Visual Studio | 2022 (Community/Professional/Enterprise) |
| مجموعة أدوات MSVC | v143 |
| Windows SDK | 10.0 (مُثبَّتة مع VS) |
| CMake (اختياري) | 3.20 أو أحدث |

> **ملاحظة:** إضافة Deep Glow **غير** مطلوبة ولا مُضمَّنة.  
> الإضافة تستخدم Glow مدمجًا خاصًا بها بدون أي اعتمادية خارجية.

---

## 1. تثبيت Visual Studio 2022

1. حمّل **Visual Studio 2022** من:  
   `https://visualstudio.microsoft.com/vs/`

2. خلال التثبيت، فعّل حزمة العمل:  
   **Desktop development with C++**

   تتضمّن تلقائيًا:
   - مُجمِّع MSVC v143
   - Windows SDK 10.0
   - CMake (اختياري)

3. تأكد من اختيار **x64** كمنصة افتراضية.

---

## 2. تثبيت AE SDK وضبط المتغير البيئي

1. سجّل دخولك إلى **Adobe Developer Console**:  
   `https://developer.adobe.com/after-effects/`

2. حمّل **After Effects SDK** (إصدار 2022 أو 2024).

3. فك الضغط إلى مجلد ثابت، مثل:  
   `C:\Adobe\AfterEffectsSDK\2024`

4. اضبط المتغير البيئي **AE_SDK_ROOT** ليشير إلى هذا المجلد.  
   افتح PowerShell **كمستخدم عادي** ونفّذ:

   ```powershell
   [System.Environment]::SetEnvironmentVariable(
       'AE_SDK_ROOT',
       'C:\Adobe\AfterEffectsSDK\2024',
       'User'
   )
   ```

   أغلق نوافذ PowerShell وأعد فتحها حتى يأخذ المتغير مفعوله.

5. تحقق من الإعداد:

   ```powershell
   echo $env:AE_SDK_ROOT
   # يجب أن يُظهر: C:\Adobe\AfterEffectsSDK\2024
   ```

---

## 3. إعداد البيئة (configure)

افتح PowerShell في مجلد `IconFlowNative` ونفّذ:

```powershell
.\configure_windows.ps1
```

**ما يفعله هذا الأمر:**
- يتحقق من وجود AE SDK وملفاته الأساسية.
- يحدد PiPLtool.exe ويولّد ملف `src\IconFlowNative.rr` (مورد PiPL المُجمَّع).
- يتحقق من وجود Visual Studio 2022.
- يُجهّز المشروع للبناء.

لإعداد مشروع CMake في نفس الوقت:

```powershell
.\configure_windows.ps1 -UseCMake
```

---

## 4. البناء (Build)

### الطريقة أ: Visual Studio (مُوصى بها)

افتح `IconFlowNative.sln` في Visual Studio 2022، ثم:

1. اختر **Release | x64** من شريط الأدوات.
2. اضغط **Build → Build Solution** (أو `Ctrl+Shift+B`).

مسار الملف الناتج:

```
IconFlowNative\build\Release\IconFlowNative.aex
```

### الطريقة ب: سكريبت PowerShell

```powershell
.\build_release.ps1
```

خيارات إضافية:

```powershell
# بناء + تشغيل اختبارات الوحدة
.\build_release.ps1 -RunTests

# بناء + تثبيت مباشرة في After Effects
.\build_release.ps1 -InstallToAE

# بناء عبر CMake
.\build_release.ps1 -UseCMake -RunTests
```

### الطريقة ج: CMake مباشرةً

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DAE_SDK_ROOT="$env:AE_SDK_ROOT"
cmake --build build --config Release
```

---

## 5. مسار الملف الناتج

```
IconFlowNative\build\Release\IconFlowNative.aex
```

حجم الملف المتوقع: بين 200 كيلوبايت و 600 كيلوبايت حسب المُحسِّن.

---

## 6. التثبيت في After Effects

### التثبيت لجميع المستخدمين (يتطلب صلاحيات المسؤول)

```
%PROGRAMFILES%\Adobe\Adobe After Effects 2024\Support Files\Plug-ins\IconFlow\IconFlowNative.aex
```

افتح PowerShell **كمسؤول** ونفّذ:

```powershell
$dest = "$env:ProgramFiles\Adobe\Adobe After Effects 2024\Support Files\Plug-ins\IconFlow"
New-Item -ItemType Directory -Path $dest -Force
Copy-Item "build\Release\IconFlowNative.aex" $dest
```

### التثبيت للمستخدم الحالي فقط

```
%APPDATA%\Adobe\After Effects\(رقم الإصدار)\Plug-ins\IconFlow\IconFlowNative.aex
```

### أو استخدم السكريبت المدمج

```powershell
.\build_release.ps1 -InstallToAE
```

---

## 7. اختبارات الوحدة (Unit Tests)

اختبارات الوحدة مستقلة تمامًا عن AE SDK وتختبر دوال الرياضيات اللونية (ColorMath.h).

### البناء والتشغيل

```powershell
# عبر CMake:
cmake --build build --config Release --target ColorMathTests
.\build\Release\ColorMathTests.exe

# أو عبر السكريبت:
.\build_release.ps1 -RunTests
```

### الاختبارات المشمولة

| الدالة | ما يُختبر |
|--------|-----------|
| `u8ToF` / `fToU8` | تحويل 8-بت ← → float |
| `u16ToF` / `fToU16` | تحويل 16-بت ← → float |
| `rgbToHsl` | تحويل RGB → HSL |
| `colorDistance` | أوضاع RGB, Hue, Chroma |
| `smoothStep` | منحنى S-curve |
| `bgMatteFactor` | عتبة عزل الخلفية |
| `greenDespill` | إزالة التسرب الأخضر |
| `colorReplaceFactor` | عامل استبدال اللون |
| `applyColorReplace` | تطبيق الاستبدال مع الحفاظ على الإضاءة |
| `applyGlobalTint` | الصبغة الموحدة |
| `luminance` | معاملات Rec.709 |
| `sample5x5ARGB8` | تحصيل وسط 5×5 بكسل |

---

## 8. قائمة اختبار يدوي في After Effects

بعد تثبيت الإضافة، تحقق من النقاط التالية بعد إعادة تشغيل After Effects:

### أساسي
- [ ] تظهر الإضافة في: **Effect > IconFlow > IconFlow Native Picker**
- [ ] تُطبَّق على طبقة فيديو بدون أخطاء
- [ ] لا يحدث تعطّل (crash) عند فتح لوحة Effect Controls

### عزل الخلفية
- [ ] طبّق الإضافة على مقطع بخلفية خضراء
- [ ] فعّل **Enable BG Key** واختر **Green** من قائمة المسافة
- [ ] اضغط **Pick BG Color** — يجب أن يتحوّل المؤشر إلى علامة تقاطع (crosshair) داخل لوحة التكوين
- [ ] انقر على منطقة خضراء داخل التكوين — يجب أن يُسجَّل اللون تلقائيًا
- [ ] تحقق أن الخلفية أصبحت شفافة بعد اختيار التسامح المناسب
- [ ] تحقق أن **Ctrl+النقر** يحصل وسط بكسل 5×5
- [ ] تحقق أن **Escape** يلغي وضع الاختيار بدون تغيير اللون

### استبدال الألوان
- [ ] اضغط **Pick Source 1** واختر لونًا من التكوين
- [ ] غيّر **To 1** إلى لون مختلف
- [ ] تحقق أن الاستبدال يظهر فورًا
- [ ] فعّل **Preserve Luminance** وتحقق أن الإضاءة تبقى مستقرة
- [ ] جرّب فتحات متعددة (Slot 2, 3, …)

### الصبغة الموحدة
- [ ] فعّل **Global Tint**، اختر لونًا
- [ ] تحقق أن جميع ألوان الطبقة تنصبغ مع الحفاظ على الظلال

### التوهج
- [ ] فعّل **Enable Glow**
- [ ] اضبط Radius وIntensity
- [ ] تحقق أن التوهج يظهر على المناطق الساطعة فقط
- [ ] تحقق أن الإضافة **لا** تطلب أو تبحث عن Deep Glow

### الجودة والاستقرار
- [ ] اختبر على مقاطع 8-بت، 16-بت، و32-بت float
- [ ] تحقق من عدم التعطّل عند الإطارات الفارغة أو alpha = 0
- [ ] تحقق من صحة العمل مع دقة تصغير (downsampling) في AE
- [ ] تحقق من صحة العمل عند تحديد طبقات متعددة

---

## 9. بنية المشروع

```
IconFlowNative/
├── src/
│   ├── IconFlowNative.h        # تعريفات المعاملات ومدخل الإضافة
│   ├── IconFlowNative.cpp      # منطق الإضافة وCustom Comp UI
│   ├── ColorMath.h             # رياضيات الألوان (header-only, بدون SDK)
│   ├── Renderer.h              # تعريف خط الرسم
│   ├── Renderer.cpp            # خط رسم متعدد أعماق البت
│   └── IconFlowNative.rc       # موارد Windows (PiPL + معلومات الإصدار)
├── pipl/
│   └── IconFlowNative.r        # مصدر PiPL (يُولّد .rr عبر PiPLtool)
├── tests/
│   ├── ColorMathTests.cpp      # اختبارات وحدة مستقلة
│   └── ColorMathTests.vcxproj  # مشروع VS للاختبارات
├── IconFlowNative.vcxproj      # مشروع Visual Studio 2022
├── IconFlowNative.sln          # ملف الحل (Solution)
├── CMakeLists.txt              # بديل CMake
├── configure_windows.ps1       # سكريبت الإعداد
├── build_release.ps1           # سكريبت البناء
└── README_AR.md                # هذا الملف
```

---

## ملاحظات تقنية مهمة

### تحويل الإحداثيات في Custom Comp UI

قطّارة اللون تعمل عبر **Custom Comp UI** من AE SDK:
- `PF_Event_ADJUST_CURSOR` → يحوّل المؤشر إلى علامة تقاطع
- `PF_Event_DO_CLICK` → يحصل إحداثيات النقرة من لوحة التكوين
- يُحوّل الإحداثيات من فضاء التكوين إلى فضاء طبقة البكسل عبر `event_extra->cbs.comp_to_layer()`

راجع التعليقات في `src/IconFlowNative.cpp` (قسم `HandleEvent`) للاطلاع على نقاط عدم اليقين التي تعتمد على إصدار SDK.

### ترتيب المعالجة

1. عزل الخلفية (alpha matte)
2. تنظيف الحافة / choke / إزالة التسرب الأخضر (spatial pass)
3. استبدال الألوان (حتى 8 فتحات)
4. الصبغة الموحدة (اختياري)
5. التوهج (Gaussian blur إضافي)

### أعماق البت المدعومة

| العمق | النوع | النطاق |
|-------|-------|--------|
| 8-بت | `PF_Pixel` | 0 – 255 |
| 16-بت | `PF_Pixel16` | 0 – 32768 |
| 32-بت float | `PF_PixelFloat` | 0.0 – 1.0+ (HDR) |
