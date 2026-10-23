# R8 rules for the release build (isMinifyEnabled = true).

# JNI: native/src/jni_onair.cpp exports Java_io_github_adswill_onair_OnAirNative_<name>. The class name, the object instance and every
# native method name and signature must survive shrinking and obfuscation, or System.loadLibrary works and the first call fails with
# UnsatisfiedLinkError.
-keep class io.github.adswill.onair.OnAirNative { *; }
-keep class io.github.adswill.onair.usb.RadioNative { *; }   # native/src/jni_radio.cpp (USB, network radios, sound-card push)
-keepclasseswithmembernames,includedescriptorclasses class * {
    native <methods>;
}

# Mode panels found with java.util.ServiceLoader (ui/panels/ModePanels.kt): keep every provider and its no-argument constructor;
# R8 rewrites META-INF/services entries for kept classes.
-keep class * implements io.github.adswill.onair.ui.panels.ModePanelProvider { public <init>(); }
-keepnames interface io.github.adswill.onair.ui.panels.ModePanelProvider

# The C++ side never calls back into Kotlin today. A future callback (FindClass / GetMethodID) needs its class and members kept here.

# Readable crash reports: keep file names and line numbers, hide the original source file name.
-keepattributes SourceFile,LineNumberTable
-renamesourcefileattribute SourceFile
