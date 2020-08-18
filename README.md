Helper that allows attach Visual Studio to process via C++ code

Known limitations:  
Doesn't work if Visual Studio and process have different permissions, e.g. one of them was ran as administrator  
Can hang if call AttachToProcess from DllMain 
