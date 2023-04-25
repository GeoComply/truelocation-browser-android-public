# truelocation-browser-android

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

#### Before run build release, you should setup Global Properties

Create or open file gradle.properties in folder ~/.gradle. Add this line:

```
oobee_keystore_password=your_keystore_pass
oobee_key_alias=your_keystore_alias
oobee_key_password=your_keystore_pass
oobee_keystore=/path/to/your/keystore
```


#### If You see this error “Unable to determine the current character, it is not a string, number, array, or object”, pls try again.

#### If You still can't build successfully, you should check java version. Please update to Java 11 and try again.
