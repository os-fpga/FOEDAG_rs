# FOEDAG_rs
Raptor GUI, reuses elements of the open-source [FOEDAG](https://github.com/os-fpga/FOEDAG) and augments it

INSTALL Instructions:
```
  git clone https://github.com/os-fpga/FOEDAG_rs.git
  make 
  make debug
  make test
  sudo make install
```

To force submodule update:
```
  make UPDATE_SUBMODULES=ON
```

To build in production mode:
```
  make PRODUCTION_BUILD=ON
```
