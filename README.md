# BADChIMP-cpp
## Build and compile 
**Linux:** Run `./make.sh <name_of_folder_with_main_file>`, in root folder, `std_case` is built if no argument to make.sh is given.  
This script will make a ```build``` folder, run ```cmake``` from that folder and then run ```make```. This can also be done by hand:
```shell
/BADChIMP-cpp$ mkdir <build-folder-name>
/BADChIMP-cpp$ cd <build-folder-name>
/BADChIMP-cpp$ cmake -DLBMAIN:STRING="<name_of_folder_with_main_file>" ./..
/BADChIMP-cpp$ make
``` 

**Windows:** Make sure that open [MPI is installed](https://docs.microsoft.com/en-us/archive/blogs/windowshpc/how-to-compile-and-run-a-simple-ms-mpi-program). Download and run `msmpisetup.exe` and `msmpisdk.msi`.  Install [cmake for Windows](https://cmake.org/). Run cmake from root directory to generate Visual Studio C++ project, or simply use VSCode.

### Podman/Docker setup  
Run `podman build -v /etc/ssl/certs:/etc/ssl/certs .`  

The `-v` option only is relevant if you have a tls mitm setup. Otherwise I encourage removing the set env variables
from the Dockerfile and not using the `-v` option.  
Did not manage to think of a cleaner way to set it up.

Use the resulting image and its id to run a container  

`podman run -it -v /etc/ssl/certs:/etc/ssl/certs ${imageid}`  

This drops you into /data with the basic setup being provided to you. The env is designed to be as minimal as possible from the POV of docker/podman.  
Additional dependencies can be pulled with `nix-shell -p vim htop` (Alternatively add more software). This drops you into a new shell which you can exit via `ctrl+d` or `exit`. But everything regarding just running the software should be available.  
