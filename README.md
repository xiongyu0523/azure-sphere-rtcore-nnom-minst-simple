# Azure Sphere RTcore NNOM MINST-simple demo

This sample demonstrates running a Neural Network on Azure Sphere RTcore. It is a port of [NNOM](https://github.com/majianjia/nnom) inference library and MINST simple demo by Jianjia Ma. Refer to this [page](https://github.com/majianjia/nnom/blob/master/docs/example_mnist_simple_cn.md) for more informaiton about MINST and this demo. 


To use this sample, clone the repository locally if you haven't already done so:

```
git clone https://github.com/xiongyu0523/azure-sphere-rtcore-nnom-minst-simple.git
```

## NOTE

1. The Cortex-M4F core is running at 26MHz out of reset, the example boost core frequency to 197.6MHz before we start FreeRTOS kernel. 
   
   ```
   uint32_t val = ReadReg32(IO_CM4_RGU, 0);
   val &= 0xFFFF00FF;
   val |= 0x00000200;
   WriteReg32(IO_CM4_RGU, 0, val);
   ```

2. To use this demo with FreeRTOS GCC Cortex-M4F port, need modify the gcc compiler flag to use FPU instructions and discard unused variables and functions at linker stage. Just copy the ***AzureSphereRTCoreToolchainVFP.cmake*** file into the Azure Sphere SDK install folder. (Default path is *C:\Program Files (x86)\Microsoft Azure Sphere SDK\CMakeFiles*)

3. To achieve high performance, **'ARM-Release'** configuraiton need to be selected in Visual Studio. 


## To build and run the sample

### Prep your device

1. Ensure that your Azure Sphere device is connected to your PC, and your PC is connected to the internet.
2. Even if you've performed this set up previously, ensure that you have Azure Sphere SDK version 19.05 or above. In an Azure Sphere Developer Command Prompt, run **azsphere show-version** to check. Download and install the [latest SDK](https://aka.ms/AzureSphereSDKDownload) as needed.
3. Right-click the Azure Sphere Developer Command Prompt shortcut and select **More > Run as administrator**.
4. At the command prompt issue the following command:

   ```
   azsphere dev prep-debug --enablertcoredebugging
   ```

   This command must be run as administrator when you enable real-time core debugging because it installs USB drivers for the debugger.
5. Close the window after the command completes because administrator privilege is no longer required.  

### Build and deploy the application

1. Start Visual Studio.
2. From the **File** menu, select **Open > CMake...** and navigate to the folder that contains the sample.
3. Select **'ARM-Release'** configuration
4. Select the file CMakeLists.txt and then click **Open**. 
5. In Solution Explorer, right-click the CMakeLists.txt file, and select **Generate Cache for Azure_Sphere_RTcore_NNOM_MINST_Simple**. This step performs the cmake build process to generate the native ninja build files. 
6. In Solution Explorer, right-click the *CMakeLists.txt* file, and select **Build** to build the project and generate .imagepackage target.
7. Double click *CMakeLists.txt* file and press F5 to start the application with debugging. LED1 will blink red. Press button A to change the blink rate.
8. The demo will print message "FreeRTOS demo" after boot via IO0_TXD (Header3-6) and receive input from ISU0_RXD (Header2-1). After reboot, the informative print is showed on terminal
   ```
    nnom minist-simple demo on Azure Sphere RTcore
    Input number 0 - 9 to feed img[0] - img[9] from test dataset to pre-generated model

    NNoM version 0.3.0
    Start compiling model...
    Layer(#)         Activation    output shape    ops(MAC)   mem(in, out, buf)      mem blk lifetime
    -------------------------------------------------------------------------------------------------
    #1   Input      -          - (  28,  28,   1,)          (   784,   784,     0)    1 - - -  - - - -
    #2   Conv2D     - ReLU     - (  28,  28,  12,)      84k (   784,  9408,    36)    1 1 1 -  - - - -
    #3   MaxPool    -          - (  14,  14,  12,)          (  9408,  2352,     0)    1 1 1 -  - - - -
    #4   Conv2D     - ReLU     - (  14,  14,  24,)     508k (  2352,  4704,   432)    1 1 1 -  - - - -
    #5   MaxPool    -          - (   7,   7,  24,)          (  4704,  1176,     0)    1 1 1 -  - - - -
    #6   Conv2D     - ReLU     - (   7,   7,  48,)     508k (  1176,  2352,   864)    1 1 1 -  - - - -
    #7   MaxPool    -          - (   4,   4,  48,)          (  2352,   768,     0)    1 1 1 -  - - - -
    #8   Dense      - ReLU     - (  96,          )      73k (   768,    96,  1536)    1 1 1 -  - - - -
    #9   Dense      -          - (  10,          )      960 (    96,    10,   192)    1 1 1 -  - - - -
    #10  Softmax    -          - (  10,          )          (    10,    10,     0)    1 1 - -  - - - -
    #11  Output     -          - (  10,          )          (    10,    10,     0)    1 - - -  - - - -
    -------------------------------------------------------------------------------------------------
    Memory cost by each block:
    blk_0:1536  blk_1:2352  blk_2:9408  blk_3:0  blk_4:0  blk_5:0  blk_6:0  blk_7:0
    Total memory cost by network buffers: 13296 bytes
    Compling done in 138 ms
   ```
    
9.  Once a '0' - '9' character is pressed on keyboard, the demo will feed one of the pre installed img[0] - img[9] input to inference engine. You can observe result from terminal.

    ```
    prediction start on img[5]...


                        ''[[CC@@WWqqjj
                    ^^vv**@@WWpp##aaBBQQ""
                    ``pp@@%%00>>  ""''}}@@bb''
                <<qq@@ZZ11          ..UU@@^^
                pp@@uu..              ::@@>>
                ddqq                    @@//
                ppCC                  ;;@@~~
                pp88ii                rr@@^^
                YY@@oo__^^          ++&&pp..
                ..LL@@@@88OOOOrr11\\&&@@<<
                    ~~ccww@@@@@@@@@@@@@@
                            ^^}}////ffWW@@
                                    II@@@@
                                    II@@@@
                                    --@@ww
                                    YYBB<<
                                    aaWW
                                ::WWWW
                                <<@@pp
                                jj@@tt


    Time: 36 tick
    Truth label: 9
    Predicted label: 9
    Probability: 100%

    Print running stat..
    Layer(#)        -   Time(us)     ops(MACs)   ops/us
    --------------------------------------------------------
    #1        Input -         0
    #2       Conv2D -         0          84k
    #3      MaxPool -         0
    #4       Conv2D -         0         508k
    #5      MaxPool -         0
    #6       Conv2D -         0         508k
    #7      MaxPool -         0
    #8        Dense -         0          73k
    #9        Dense -         0          960
    #10     Softmax -         0
    #11      Output -         0

    Summary:
    Total ops (MAC): 1175424(1.17M)
    Prediction time :0us
    Total memory:15036
    ```



