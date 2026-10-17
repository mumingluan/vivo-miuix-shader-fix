package com.axuan.vivomiuix;
import java.io.*;
import java.lang.reflect.*;
import java.util.*;

/** Discovers the Shader contract and exact boot-framework callers, independent of OEM method names. */
public final class ShaderContracts {
    private ShaderContracts() { }
    static Method target(Class<?> shader) throws NoSuchMethodException {
        Method found=null;
        for(Method method:shader.getDeclaredMethods()) {
            if(Modifier.isStatic(method.getModifiers())||Modifier.isAbstract(method.getModifiers()))continue;
            Class<?>[] parameters=method.getParameterTypes();
            if(method.getReturnType()!=shader||parameters.length!=1||parameters[0]!=boolean.class)continue;
            if(found!=null)throw new NoSuchMethodException("Ambiguous Shader copy contract");
            found=method;
        }
        if(found==null)throw new NoSuchMethodException("Missing Shader copy contract");
        found.setAccessible(true);return found;
    }
    static List<DexIndex.Ref> callers(List<File> files,DexIndex.Ref target)throws IOException {
        Set<DexIndex.Ref> result=new LinkedHashSet<>(), forwarded=new LinkedHashSet<>();forwarded.add(target);
        boolean changed;
        do {changed=false;for(File file:files) {
            // One archive at a time; raw DEX readers and unrelated method facts
            // must not remain attached to the manager process after discovery.
            DexIndex index=DexIndex.readApks(Collections.singletonList(file),false);
            for(DexIndex.MethodData method:index.methods.values()) {
                boolean used=false;for(DexIndex.Ref callee:forwarded)if(method.calls(callee)){used=true;break;}
                if(!used)continue;
                result.add(method.ref);
                // Small Shader overloads may have been inlined before the
                // boolean overload. Include their callers as well.
                if(method.ref.owner.equals(target.owner)&&method.ref.type.endsWith(")"+target.owner)
                        && !forwarded.contains(method.ref)) {
                    DexIndex.Facts facts=method.facts();
                    if(facts.instructions<=12&&facts.calls.size()==1&&forwarded.containsAll(facts.calls))
                        changed|=forwarded.add(method.ref);
                }
            }
            index.methods.clear();index.parents.clear();
        }}while(changed);
        if(result.isEmpty())throw new IOException("No boot-framework callers of discovered Shader contract");
        return new ArrayList<>(result);
    }
    public static Object[] discover()throws Exception {
        Class<?> shader=Class.forName("android.graphics.Shader");Method target=target(shader);
        DexIndex.Ref ref=new DexIndex.Ref("Landroid/graphics/Shader;",target.getName(),"(Z)Landroid/graphics/Shader;");
        Set<File> files=new LinkedHashSet<>();
        for(String paths:Arrays.asList(System.getenv("BOOTCLASSPATH"),System.getProperty("java.boot.class.path"))) {
            if(paths==null)continue;
            for(String path:paths.split(java.util.regex.Pattern.quote(File.pathSeparator))) {
                File file=new File(path);if(file.isFile())files.add(file.getCanonicalFile());
            }
        }
        if(files.isEmpty())throw new IOException("No readable boot classpath archives");
        List<Member> methods=new ArrayList<>();
        for(DexIndex.Ref caller:callers(new ArrayList<>(files),ref)) {
            Class<?> owner=type(caller.owner,shader.getClassLoader());Class<?>[] parameters=parameters(caller.type,shader.getClassLoader());
            if(caller.name.equals("<init>"))methods.add(owner.getDeclaredConstructor(parameters));
            else if(!caller.name.equals("<clinit>"))methods.add(owner.getDeclaredMethod(caller.name,parameters));
        }
        if(methods.isEmpty())throw new IOException("No deoptimizable Shader caller members");
        return new Object[]{target,methods.toArray(new Member[0])};
    }
    private static Class<?>[] parameters(String descriptor,ClassLoader loader)throws ClassNotFoundException {
        List<Class<?>> result=new ArrayList<>();int index=1;
        while(descriptor.charAt(index)!=')') {
            int start=index;while(descriptor.charAt(index)=='[')index++;
            index=descriptor.charAt(index)=='L'?descriptor.indexOf(';',index)+1:index+1;
            if(index<=start)throw new ClassNotFoundException("Invalid DEX descriptor");
            result.add(type(descriptor.substring(start,index),loader));
        }
        return result.toArray(new Class<?>[0]);
    }
    private static Class<?> type(String descriptor,ClassLoader loader)throws ClassNotFoundException {
        switch(descriptor) {
            case "Z":return boolean.class;case "B":return byte.class;case "C":return char.class;
            case "S":return short.class;case "I":return int.class;case "J":return long.class;
            case "F":return float.class;case "D":return double.class;case "V":return void.class;
            default:return Class.forName(descriptor.charAt(0)=='L'?descriptor.substring(1,descriptor.length()-1).replace('/','.')
                    :descriptor.replace('/','.'),false,loader);
        }
    }
}
