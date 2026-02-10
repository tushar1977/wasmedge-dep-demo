#include "iostream"
#include "set"
#include <map>
#include <vector>
#include <wasmedge/wasmedge_basic.h>
#include <wasmedge/wasmedge_configure.h>
#include <wasmedge/wasmedge_context.h>
#include <wasmedge/wasmedge_vm.h>

class DependencyTracker {
private:
  std::map<std::string, std::set<std::string>> dependsOn;
  std::map<std::string, std::set<std::string>> dependedOnBy;

public:
  void addDependency(const std::string &srcModule,
                     const std::string &providerModule) {
    dependsOn[srcModule].insert(providerModule);
    dependedOnBy[providerModule].insert(srcModule);

    if (dependsOn.find(providerModule) == dependsOn.end()) {
      dependsOn[providerModule] = {};
    }
    if (dependedOnBy.find(srcModule) == dependedOnBy.end()) {
      dependedOnBy[srcModule] = {};
    }
  }

  std::vector<std::string> getDependents(const std::string &moduleName) const {
    auto it = dependedOnBy.find(moduleName);
    if (it == dependedOnBy.end()) {
      return {};
    }
    return std::vector<std::string>(it->second.begin(), it->second.end());
  }

  bool canSafelyRemove(const std::string &moduleName) const {
    return getDependents(moduleName).empty();
  }

  void removeModule(const std::string &moduleName) {
    auto depsIt = dependsOn.find(moduleName);
    if (depsIt != dependsOn.end()) {
      for (const auto &provider : depsIt->second) {
        dependedOnBy[provider].erase(moduleName);
      }
      dependsOn.erase(depsIt);
    }
    dependedOnBy.erase(moduleName);
  }

  void printGraph() const {

    for (const auto &[module, deps] : dependsOn) {
      std::cout << "\nModule: '" << module << "'\n";

      if (!deps.empty()) {
        std::cout << "Imports from: ";
        for (const auto &dep : deps) {
          std::cout << "'" << dep << "' ";
        }
        std::cout << "\n";
      }

      auto dependents = getDependents(module);
      if (!dependents.empty()) {
        std::cout << "Imported by: ";
        for (const auto &dep : dependents) {
          std::cout << "'" << dep << "' ";
        }
        std::cout << "\n";
      }
    }
    std::cout << "\n";
  }

  bool hasModule(const std::string &moduleName) const {
    return dependsOn.find(moduleName) != dependsOn.end();
  }
};

class WasmRuntime {
private:
  WasmEdge_ConfigureContext *ConfCtx = NULL;
  WasmEdge_VMContext *VMCtx = NULL;
  DependencyTracker depTracker;

public:
  WasmRuntime() {
    ConfCtx = WasmEdge_ConfigureCreate();

    WasmEdge_ConfigureAddHostRegistration(ConfCtx,
                                          WasmEdge_HostRegistration_Wasi);
    VMCtx = WasmEdge_VMCreate(ConfCtx, NULL);
  }

  bool registerModule(const char *name, const char *path) {
    WasmEdge_String ModuleName = WasmEdge_StringCreateByCString(name);
    WasmEdge_Result Res =
        WasmEdge_VMRegisterModuleFromFile(VMCtx, ModuleName, path);
    WasmEdge_StringDelete(ModuleName);

    if (!WasmEdge_ResultOK(Res)) {
      std::cerr << "Module registration failed: "
                << WasmEdge_ResultGetMessage(Res) << "\n";
      return false;
    }

    // Creating Nodes in graph
    depTracker.addDependency(name, name);
    depTracker.removeModule(name);
    return true;
  }

  bool loadAndInstantiate(const char *path, const char *moduleName,
                          const std::vector<std::string> &dependencies = {}) {
    WasmEdge_Result Res = WasmEdge_VMLoadWasmFromFile(VMCtx, path);
    if (!WasmEdge_ResultOK(Res)) {
      std::cerr << "Load failed: " << WasmEdge_ResultGetMessage(Res) << "\n";
      return false;
    }

    Res = WasmEdge_VMValidate(VMCtx);
    if (!WasmEdge_ResultOK(Res)) {
      std::cerr << "Validation failed: " << WasmEdge_ResultGetMessage(Res)
                << "\n";
      return false;
    }

    Res = WasmEdge_VMInstantiate(VMCtx);
    if (!WasmEdge_ResultOK(Res)) {
      std::cerr << "Instantiation failed: " << WasmEdge_ResultGetMessage(Res)
                << "\n";
      return false;
    }

    depTracker.addDependency(moduleName, moduleName);
    depTracker.removeModule(moduleName);

    for (const auto &dep : dependencies) {
      std::string depName = dep;
      size_t lastSlash = dep.find_last_of('/');
      if (lastSlash != std::string::npos) {
        depName = dep.substr(lastSlash + 1);
      }
      size_t dotPos = depName.find_last_of('.');
      if (dotPos != std::string::npos) {
        depName = depName.substr(0, dotPos);
      }

      // Adding relations
      depTracker.addDependency(moduleName, depName);
      std::cout << "Tracked dependency: '" << moduleName << "' imports from '"
                << depName << "'\n";
    }
    return true;
  }

  int64_t executeAddAndSquare(int64_t a, int64_t b) {
    WasmEdge_String funcName = WasmEdge_StringCreateByCString("add_and_square");

    WasmEdge_Value params[2] = {WasmEdge_ValueGenI64(a),
                                WasmEdge_ValueGenI64(b)};
    WasmEdge_Value returns[1];

    WasmEdge_Result res =
        WasmEdge_VMExecute(VMCtx, funcName, params, 2, returns, 1);

    WasmEdge_StringDelete(funcName);

    if (!WasmEdge_ResultOK(res)) {
      std::cerr << "Execution failed: " << WasmEdge_ResultGetMessage(res)
                << "\n";
      return 0;
    }

    return WasmEdge_ValueGetI64(returns[0]);
  }
  ~WasmRuntime() {
    if (VMCtx)
      WasmEdge_VMDelete(VMCtx);
    if (ConfCtx)
      WasmEdge_ConfigureDelete(ConfCtx);
  }

  void printDependencyGraph() const { depTracker.printGraph(); }
};

int main() {
  WasmRuntime runtime;

  if (!runtime.registerModule("lib", "modules/lib.wasm"))
    return -1;
  std::vector<std::string> calcDeps = {"lib"};
  if (!runtime.loadAndInstantiate("modules/calc.wasm", "calc", calcDeps))
    return -1;

  runtime.printDependencyGraph();

  int result = runtime.executeAddAndSquare(4, 5);
  std::cout << "\nExecution Result: " << result << std::endl;

  return 0;
}
