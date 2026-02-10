## Folder Structure
```
wasmedge-dep-demo/
├── host.cpp # Main runtime implementation
├── modules/ # WebAssembly module directory
│ ├── lib.wat # WAT text format of lib.wasm
│ └── calc.wat # WAT text format of calc.wasm
├── README.md # This file
```

## How to run this
1) First compile wat to wasm
```bash
cd modules
wat2wasm lib.wat > lib.wasm
wat2wasm calc.wat > calc.wasm
```
2) Compile cpp file using g++ or clangd and make sure to include Wasmedge C/C++ include file
```bash
g++ -lwasmedge -o host host.cpp
```
3) Run the host file
```bash
./host
```

4) Desired Output
```bash
➜ ./host
Tracked dependency: 'calc' imports from 'lib'

Module: 'calc'
Imports from: 'lib' 

Module: 'lib'
Imported by: 'calc' 


Execution Result: 81
```
