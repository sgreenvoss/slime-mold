**Experiments with slime mold**

This repo represents an initial stab at what will hopefully be a longer project, in which I develop a quick, lightweight slime mold simulator. For now, there are two branches, each of which represents an approach to simulation.

**CPU**

Located in the main branch, this implementation lives solely on the CPU. The simulations run much slower and can support many fewer particles, but since programming on the CPU is easier (for me), I was able to implement more interesting features. Two or more independent simulations can run on top of one another, as can be seen in "ventricles" below. The slimes can also prefer higher or lower values in the world, and will therefore fight to keep their regions as high/low as they can, as shown in "green city." While this preference would likely be trivial to implement using compute shaders, I ran out of time and stayed up too late to be able to get around to it.  

The CPU implementation can be configured by changing the const string world_type to correspond to a different world. Inside the configuration json file, further configuration can be done by adding, changing, and combining different slime types into new worlds.

Obviously the immediate next step for this branch would be to give it the same configurable executable as in the compute branch, since that would allow users to run different simulations without recompiling every time. 

**green city**: two slimes with similar parameters but who each want overall negative vs positive

<img width="265" alt="Screenshot 2025-12-08 094014" src="https://github.com/user-attachments/assets/6e7d0b86-0463-4dd5-93ab-ed1b6992a081" />
<img width="265"  alt="Screenshot 2025-12-08 094022" src="https://github.com/user-attachments/assets/3ea79021-236e-4a11-ac2e-3d1982884c14" />
<img width="265"  alt="Screenshot 2025-12-08 094034" src="https://github.com/user-attachments/assets/d05bfaec-9632-4449-b3f5-a0c52d8eca8c" />

**purple haze**: two slimes with the same preferences, one with different preferences.

<img width="265" alt="Screenshot 2025-12-08 094315" src="https://github.com/user-attachments/assets/cb73ad72-5231-4be2-a6bf-0e1af40ddb88" />
<img width="265"  alt="Screenshot 2025-12-08 094323" src="https://github.com/user-attachments/assets/5458a246-2fcd-474c-82b4-5102ba193288" />
<img width="265" alt="Screenshot 2025-12-08 094349" src="https://github.com/user-attachments/assets/d81e6463-c4ce-4ead-808e-178fd58be092" />

**ventricles**: a red slime layered on top of green city

<img width="265" alt="Screenshot 2025-12-08 094629" src="https://github.com/user-attachments/assets/e34b0543-6910-4181-b8cc-58668dad6ab8" />
<img width="265" alt="Screenshot 2025-12-08 094643" src="https://github.com/user-attachments/assets/2fa491ab-b9be-4e50-a015-92761b201313" />
<img width="265" alt="Screenshot 2025-12-08 094713" src="https://github.com/user-attachments/assets/a33d71f1-704e-45bb-ab69-a918a5017c49" />

**GPU**

The other portion of this repo is found on the branch compute, and it aims to use compute shaders to achieve the same thing as before, but much faster. I found this to be much more difficult. Additionally, since the addition of two(ish) new variables (the diffusion amount and the greatly increased possibility for canvas width, height, and number of slimes) represents a huge growth in the potential exploration space, I was not able to come up with as cool of results as I would have liked. I also think that there may be some logical bug somewhere, since the configurations that I currently have are less slime mold-like than the CPU configurations. Nevertheless, I think that the current configurations (shown below) are interesting enough. 

Two compute shaders are used, one that is called once per slime and one that is called once per pixel. Much of the simulation logic is then broken out into these two shaders. I also added functionality for changing simulation type during execution. 

For the future, I would love to add proper avoidance logic, since currently the slimes are only configured to ignore trails that don't match their color code. I also want to implement some more functionality, such as the infection mechanic described by Sage Jenson (see link below) or evolutionary behavior. Reading from an image to create an initial trail map texture would also be fantastic. 

**red mold**: seems to prefer to move from high complexity lines to low complexity + small offshoots

<img width="265"  alt="image" src="https://github.com/user-attachments/assets/e8569941-4445-4948-b193-3345df823ee3" />
<img width="265"  alt="image" src="https://github.com/user-attachments/assets/83d75540-de62-4d50-944b-9a2715f89760" />
<img width="265" alt="Screenshot 2025-12-08 100016" src="https://github.com/user-attachments/assets/cc7feb3b-daba-4e92-9158-240bd9be44dc" />


**redgreen lines**: two of the same configs layered on top of one another. 

<img width="265"  alt="Screenshot 2025-12-08 100132" src="https://github.com/user-attachments/assets/5915b5ba-c047-4bed-a7dc-e412d6f74f8d" />
<img width="265"  alt="Screenshot 2025-12-08 100144" src="https://github.com/user-attachments/assets/eef4ca43-43e6-4c2a-9c0c-025a62be3db3" />
<img width="265" alt="Screenshot 2025-12-08 100220" src="https://github.com/user-attachments/assets/de9342a8-bafb-474d-97bb-dc6147d675f3" />


**very strange**: an unpredictable configuration

<img width="265"  alt="Screenshot 2025-12-08 100513" src="https://github.com/user-attachments/assets/17a43c14-5511-459e-93c8-44da4aed5f72" />
<img width="265"  alt="Screenshot 2025-12-08 100521" src="https://github.com/user-attachments/assets/369132ff-7b8c-478a-970d-5048106b3f35" />
<img width="265"  alt="Screenshot 2025-12-08 100532" src="https://github.com/user-attachments/assets/8f43adb8-0fad-4efb-b8e3-32f8e2c90e21" />
<img width="265"  alt="Screenshot 2025-12-08 100547" src="https://github.com/user-attachments/assets/36c80918-1f20-4377-827e-0ca73febc247" />
<img width="265" alt="Screenshot 2025-12-08 100559" src="https://github.com/user-attachments/assets/0099f0ca-7074-4123-98f5-30b4fbb162c2" />
<img width="265" alt="Screenshot 2025-12-08 100618" src="https://github.com/user-attachments/assets/79aa35ce-7b9c-49c7-9399-8649c196386e" />



**purple haze**: a misnomer

<img width="265" alt="Screenshot 2025-12-08 100923" src="https://github.com/user-attachments/assets/aadebad8-d0fc-4d93-8e1c-0edf5e4d9eb3" />
<img width="265" alt="Screenshot 2025-12-08 100932" src="https://github.com/user-attachments/assets/0b33c447-58c2-4385-96c1-695c5880708c" />
<img width="265" alt="Screenshot 2025-12-08 101008" src="https://github.com/user-attachments/assets/0c6d265d-2474-4c32-afd0-1dd4edaac4eb" />



**wheel**: strangely has spokes?

<img width="413" alt="Screenshot 2025-12-08 101116" src="https://github.com/user-attachments/assets/b4969c06-798c-43fc-bb66-bbc12c807679" />
<img width="413" alt="Screenshot 2025-12-08 101128" src="https://github.com/user-attachments/assets/f0f43674-f301-4816-89d2-8acb722dd56a" />
<img width="413" alt="Screenshot 2025-12-08 101151" src="https://github.com/user-attachments/assets/8102595a-b42a-4a5e-88b9-5cf601e961fd" />
<img width="413" alt="Screenshot 2025-12-08 101236" src="https://github.com/user-attachments/assets/d8de5290-3d35-4061-b563-37f1448839ea" />


**supernova**: layering with **very strange** above

<img width="413" alt="Screenshot 2025-12-08 101746" src="https://github.com/user-attachments/assets/d62d19c9-17e3-419c-a5e7-1b27b961169a" />
<img width="413" alt="Screenshot 2025-12-08 101820" src="https://github.com/user-attachments/assets/c34567f2-b29c-479a-80c6-5637659d281e" />
<img width="413" alt="Screenshot 2025-12-08 101843" src="https://github.com/user-attachments/assets/6a36de76-a43b-407e-a1b2-97f6fb68a5ff" />
<img width="413" alt="Screenshot 2025-12-08 101923" src="https://github.com/user-attachments/assets/fd21657c-17f1-4481-80fb-b3b5eed01f02" />



Further information / references: 
- Jeff Jones, with the Centre for Unconventional Computing at the University of the West of England (Characteristics of Pattern Formation and Evolution in Approximations of Physarum Transport Networks)
- Sage Jenson (https://cargocollective.com/sagejenson/physarum, https://www.sagejenson.com/36points/)
- Daniel Coady (https://medium.com/@daniel.coady/compute-shaders-in-opengl-4-3-d1c741998c03)
- Sebastian Lague (https://github.com/SebLague, https://www.youtube.com/watch?v=X-iSQQgOd1A)

