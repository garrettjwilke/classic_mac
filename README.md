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

Mini vMac requires the correct ROM file to be in the same directory as the application itself. the rom is in the minivmac tar.

You can create custom Mini vMac applications here:<br>
[Mini vMac - Variations Service (Advanced)](https://www.gryphel.com/c/minivmac/vara_srv.html)

