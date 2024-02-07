# Ray marching fluid renderer

![an image of the application in action](https://github.com/Fruup/bachelor-thesis-web/blob/4b88b056ef06eecfcccf12028b35aafd22c439d0/static/figures/figure-result-4.png?raw=true)

---

This is the repository for my bachelor's thesis with the unweildy name _Implementation of a method of surface reconstruction and visualization of particle-based fluids_.

I implemented a ray marching renderer for fluids using Vulkan. The ray marching algorithm currently runs on the CPU as I weren't able to implement a GPU implementation in the time constraints of the thesis.

# Where to read the thesis

You can find an online version of my thesis here: https://thesis.leonscherer.com ([repo](https://github.com/Fruup/bachelor-thesis-web))

# Running the project

You have to clone the repository using `--recurse-submodules` as it uses submodules for some of its dependencies.

The workspace files can be generated using `premake5` (see vendor/premake). On Windows, the script `premake.ps1` can be executed as a shorthand.

Now the Visual Studio project can be opened and executed. It is advised to use Release mode to speed up the rendering.
