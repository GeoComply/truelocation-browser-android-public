# truelocation-browser-android

**Important Notice**

This repository is provided for illustrative and reference purposes only. It provides Firefox browser source code with modifications to support build of TrueLocation Browser application. **Do not use this code in production or attempt to compile the application** as it does not contain the proprietary source code required to build the TrueLocation Browser.
No guarantees, warranties or support are provided for the code in this repository. For security submissions, only the latest version of the TrueLocation Browser application available on Google Play is eligible for review. Please do not submit security reports for this repository or any unofficial builds. To submit security reports for TrueLocation Browser, please follow guidance from https://www.geocomply.com/trust-center/security/

## Config gecko

```
cd gecko
mkdir .git
./mach bootstrap
```
Note: If you finished "./mach bootstrap" with errors, you should update new version of Python and check the python PATH. If you meet the error missing "Hg" file, you should install mercurial by: pip3 install mercurial

Note: select option 4 and y for all confirm questions, excepted question about submit code to Mozilla

After run ./mach bootstrap successful run:

```
./mach build
```



## Init android-components
Open android studio an open android-components by android studio and waiting until indexing finished. If failed pls try again.



## Init Fenix
Open android studio an open Fenix by android studio and waiting until indexing finished. If failed pls try again.

Open local.properties and add this lines

```
dependencySubstitutions.geckoviewTopsrcdir=/path/to/gecko
autoPublish.android-components.dir=/path/to/android-components
```


#### If You see this error “Unable to determine the current character, it is not a string, number, array, or object”, pls try again.

#### If You still can't build successfully, you should check java version. Please update to Java 11 and try again.
