# get it working

clone repo

```
git clone git@github.com:garrettjwilke/classic_mac.git
cd classic_mac
```

we need to compile the retro68 toolkit. if you follow the building instructions at the [Retro68 Github](https://github.com/autc04/Retro68), you should have a new directory called `Retro68-build`. Make sure the `Retro68-build` directory is in the root of the `classic_mac` repository:<br>
'classic_mac/Retro68-build'

once you have the toolkit built and in place, you can then compile using the `build.sh` script:
```
./build.sh projects/hello_world
```

if build is successful, there will be a disk file for the compiled application in the `target/00_floppy_images` directory.

---

disk jockey jr is required if you want to convert the compiled `.dsk` file into a bluescsi compatible image. install [djjr](https://diskjockey.onegeekarmy.eu/djjr/) and then open a new terminal or reload PATH if you just installed djjr.

you need to enable this by editing the `build.sh` file. change the `BLUESCSI_CONVERT` variable to `true`:
```
BLUESCSI_CONVERT=true
```

save the `build.sh` file and then run `.build.sh path/to/project` again to compile your application and convert to bluescsi `.hda`. the compiled `.dsk` file will be converted to `.hda` for bluescsi at `target/00_bluescsi_images`.

---

mini vmac requires the correct `.ROM` file to be in the same directory as the application itself. it also requires a boot disk. these files are in the `minivmac/vmac_stuffs.tar.gz`.

after getting mini vmac running, the provided boot disk will automatically load the [Launcher Application](https://github.com/autc04/Retro68/tree/master/Samples/Launcher), written by [autc04](https://github.com/autc04/Retro68).<br>
you can then use mini vmac to rapidly test your application:<br>
`edit project` > `compile` > `drag and drop into mini v mac` > `repeat`

>NOTE: mini vmac will only load `.dsk` files. it will not load the converted `.hda` files.

you can create custom mini vmac builds here:<br>
[Mini vMac - Variations Service (Advanced)](https://www.gryphel.com/c/minivmac/vara_srv.html)

the 
