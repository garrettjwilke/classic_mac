# classic mac applicaiton development
this is my attempt at making it as easy as possible to build a classic 68k or ppc macintosh application. at the time of this writing, i have only tested with mini vmac and a Macintosh SE (4MB RAM/16Mhz radius accelerator CPU/bluescsi drive).

## get it working

clone repo

```
git clone git@github.com:garrettjwilke/classic_mac.git
cd classic_mac
```

we need to compile the retro68 toolkit. if you follow the building instructions at the [Retro68 Github](https://github.com/autc04/Retro68), you should have a new directory called `Retro68-build`. Make sure the `Retro68-build` directory is in the root of the `classic_mac` repository:<br>
`classic_mac/Retro68-build`

once you have the toolkit built and in place, you can then compile using the `build.sh` script:
```
./build.sh projects/hello_world
```

if build is successful, the build files will be in the `target/hello_world` directory.<br>
there will also be a disk file for the compiled application in the `target/00_floppy_images` directory.

---

disk jockey jr is required if you want to convert the compiled `.dsk` file into a bluescsi compatible image.<br>
install [djjr](https://diskjockey.onegeekarmy.eu/djjr/) and then open a new terminal or reload PATH if you just installed djjr.
the `build.sh` script will detect if you have `djjr` in your PATH and then enable it automatically. the compiled `.dsk` file will be converted to `.hda` for bluescsi at `target/00_bluescsi_images`.

---

you can create a new project by running:
```
./create_new_project.sh
```
it will create a new folder with the name you choose to:
```
classic_mac/projects/NAME_OF_PROJECT
```
it will create a basic "Hello World!" type application (`NAME_OF_PROJECT.c`).<br>
and also a `CMakeLists.txt` file.

you can then build this project by running:
```
./build.sh path/to/NAME_OF_PROJECT
```

---

mini vmac requires the correct `.ROM` file to be in the same directory as the application itself. it also requires a boot disk. these files are in the `minivmac/vmac_stuffs.tar.gz`.

after getting mini vmac running, the provided boot disk will automatically load the [Launcher Application](https://github.com/autc04/Retro68/tree/master/Samples/Launcher), written by [autc04](https://github.com/autc04/Retro68).<br>
you can then use mini vmac to rapidly test your application:<br>
`edit project` > `compile` > `drag and drop into mini v mac` > `repeat`

>NOTE: mini vmac will only load `.dsk` files. it will not load the converted `.hda` files.

you can create custom mini vmac builds here:<br>
[Mini vMac - Variations Service (Advanced)](https://www.gryphel.com/c/minivmac/vara_srv.html)
