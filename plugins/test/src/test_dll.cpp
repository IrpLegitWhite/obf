#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>


int add(int a, int b) {
    return a + b;
}


int compute(int x) {
    int result = x * 2 + 100;
    result -= 50;
    return result;
}


int factorial(int n) {
    int result = 1;
    for (int i = 1; i <= n; ++i) {
        result *= i;
    }
    return result;
}


std::string greet(const std::string& name) {
    return "Hello, " + name + "!";
}


bool isEven(int x) {
    if (x % 2 == 0) {
        return true;
    }
    return false;
}


int sumArray(const int* arr, int n) {
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += arr[i];
    }
    return sum;
}


int getValue(const std::map<std::string, int>& m, const std::string& key) {
    auto it = m.find(key);
    if (it != m.end()) {
        return it->second;
    }
    return -1;
}


extern "C" __declspec(dllexport) int __stdcall AddNumbers(int a, int b) {
    return add(a, b);
}

extern "C" __declspec(dllexport) int __stdcall ComputeValue(int x) {
    return compute(x);
}

extern "C" __declspec(dllexport) int __stdcall Factorial(int n) {
    return factorial(n);
}

extern "C" __declspec(dllexport) int __stdcall IsEven(int x) {
    return isEven(x) ? 1 : 0;
}

extern "C" __declspec(dllexport) int __stdcall SumArray(const int* arr, int n) {
    return sumArray(arr, n);
}

extern "C" __declspec(dllexport) const char* __stdcall Greet(const char* name) {
    static std::string result;
    result = greet(name);
    return result.c_str();
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringA("DLL attached");
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringA("DLL detached");
        break;
    }
    return TRUE;
}


#ifdef BUILD_EXE
int main() {
    std::cout << "Add(3, 4) = " << add(3, 4) << std::endl;
    std::cout << "Compute(10) = " << compute(10) << std::endl;
    std::cout << "Factorial(5) = " << factorial(5) << std::endl;
    std::cout << "IsEven(4) = " << isEven(4) << std::endl;
    std::cout << "Greet(\"World\") = " << greet("World") << std::endl;

    int arr[] = { 1, 2, 3, 4, 5 };
    std::cout << "SumArray = " << sumArray(arr, 5) << std::endl;

    std::map<std::string, int> m;
    m["key1"] = 100;
    m["key2"] = 200;
    std::cout << "getValue(key1) = " << getValue(m, "key1") << std::endl;

    return 0;
}
#endif