;; calc.wasm
(module
    (type $type0 (func (param i64 i64)(result i64)))
    (import "lib" "add" (func $lib-add (type $type0)))
    (import "lib" "mul" (func $lib-mul (type $type0)))
    (func (export "add_and_square") (param i64 i64) (result i64)
        (call $lib-mul
            (call $lib-add (local.get 0) (local.get 1))
            (call $lib-add (local.get 0) (local.get 1))
        )
    )
)
