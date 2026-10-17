package com.axuan.vivomiuix;
import java.io.*;import java.util.*;
/** Uses the same scanner as the native module against supplied official framework archives. */
public final class ShaderContractProbe {
    public static void main(String[] args)throws Exception {
        List<File> files=new ArrayList<>();for(String arg:args)files.add(new File(arg));
        List<DexIndex.Ref> targets=new ArrayList<>();
        for(File file:files) {
            DexIndex index=DexIndex.readApks(Collections.singletonList(file));
            for(DexIndex.MethodData method:index.methods.values())if(!method.isStatic()
                    && method.ref.owner.equals("Landroid/graphics/Shader;")&&method.ref.type.equals("(Z)Landroid/graphics/Shader;"))targets.add(method.ref);
        }
        if(targets.size()!=1)throw new AssertionError("Missing/ambiguous Shader shape: "+targets);
        System.out.println("target="+targets.get(0));
        for(DexIndex.Ref ref:ShaderContracts.callers(files,targets.get(0))) {
            if(!ref.owner.equals("Landroid/graphics/Shader;")&&!ref.owner.equals("Lvivo/app/nightmode/NightModeController;"))
                throw new AssertionError("Caller is outside the runtime reflection scope: "+ref);
            System.out.println("caller="+ref);
        }
        // Different target names must not be accidentally treated as equivalent.
        try{ShaderContracts.callers(files,new DexIndex.Ref(targets.get(0).owner,"missing-method-probe",targets.get(0).type));}
        catch(IOException expected){System.out.println("PASS target shape, actual call sites, missing target rejection");return;}
        throw new AssertionError("Missing target accepted");
    }
}
