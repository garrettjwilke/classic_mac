# get it working

clone repo

```
git clone git@github.com:garrettjwilke/classic_mac.git
cd classic_mac
```

we need to compile the retro68 toolkit. if you follow the building instructions at the [Retro68 Github](https://github.com/autc04/Retro68), you should have a new directory called `Retro68-build`. Make sure the `Retro68-build` directory is in the root of the `classic_mac` repository.

```
classic_mac/Retro68-build
```

if you want to convert disk images to BLUESCSI, install [Disk Jockey Jr](https://diskjockey.onegeekarmy.eu/djjr/).

once you have the toolkit and disk jockey jr, you can then compile using the `build.sh` script:
```
./build.sh projects/hello_world
```

if build is successful, there will be a disk file for the compiled application in the `target/00_floppy_images` directory.

---

mini vmac requires the correct `.ROM` file to be in the same directory as the application itself. it also requires a boot disk. these files are in the `minivmac/vmac_stuffs.tar.gz`.

after getting mini vmac running, the provided boot disk will automatically load the [Launcher Application](https://github.com/autc04/Retro68/tree/master/Samples/Launcher), written by [autc04](https://github.com/autc04/Retro68).<br>
you can then use mini vmac to rapidly test your application:<br>
`create/edit application` > `compile` > `drag and drop into mini v mac` > `repeat`

you can create custom mini vmac builds here:<br>
[Mini vMac - Variations Service (Advanced)](https://www.gryphel.com/c/minivmac/vara_srv.html)

the 
