# Functional coverage with FC4SC

This example shows how to collect functional coverage in a UVM-SystemC
testbench with the Accellera [FC4SC](https://github.com/accellera-official/fc4sc)
library.

`coverage_subscriber` is a `uvm_subscriber` that owns an FC4SC covergroup.
Its `write` method samples the covergroup for each packet received from the
monitor's analysis port, and its `report_phase` reports the coverage and
stores it in the UCIS XML file `coverage_results.xml`. The file name can be
changed with the `ucis_file` setting in the configuration database.

FC4SC is not part of UVM-SystemC. The example is only built by the CMake
regression flow when `UVM_SYSTEMC_ENABLE_FC4SC` is set. FC4SC is located as
a CMake package via `CMAKE_PREFIX_PATH`, or as a plain source checkout via
`FC4SC_HOME`:

```sh
cmake -S . -B objdir \
  -DCMAKE_PREFIX_PATH=/path/to/systemc \
  -DENABLE_REGRESSION=ON \
  -DUVM_SYSTEMC_ENABLE_FC4SC=ON \
  -DFC4SC_HOME=/path/to/fc4sc
cmake --build objdir --target check-tests
```
