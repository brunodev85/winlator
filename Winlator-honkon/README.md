<p align="center">
	<img src="logo.png" width="376" height="128" alt="Winlator Logo" />  
</p>

# Winlator-honkon

Winlator es una aplicación de Android que te permite ejecutar aplicaciones de Windows (x86_64) con Wine y Box86/Box64.


# Aclaración

Winlator-Honkon es un fork modificado para VirGL,el contenido para adreno DXVK,Turnip,VKD3D y zink fue eliminado.

tener presente que VirGL es universal y funciona en cualquier dispositivo.

tener presente que VirGL usa la api gráfica OpenGL para el renderizado.

tener presente que VirGL en winlator esta limitado a DirectX 9c,esto debibo a varios factores.

recordatorio que no todos los juegos pueden ser ejecutados en winlator y menos si se usa VirGL y wined3d.


# Instalación 

1. descargar apk oficial desde el github [GitHub Releases](https://github.com/brunodev85/winlator/releases)

2. descargar apk mod-honkon desde aqui [Releases](https://github.com/Honkonx/winlator-honkon/releases)

4. Inicie la aplicación y espere a que finalice el proceso de instalación.

----

[![Play on Youtube](https://img.youtube.com/vi/ETYDgKz4jBQ/3.jpg)](https://www.youtube.com/watch?v=ETYDgKz4jBQ)
[![Play on Youtube](https://img.youtube.com/vi/9E4wnKf2OsI/2.jpg)](https://www.youtube.com/watch?v=9E4wnKf2OsI)
[![Play on Youtube](https://img.youtube.com/vi/czEn4uT3Ja8/2.jpg)](https://www.youtube.com/watch?v=czEn4uT3Ja8)
[![Play on Youtube](https://img.youtube.com/vi/eD36nxfT_Z0/2.jpg)](https://www.youtube.com/watch?v=eD36nxfT_Z0)

----

# consejos para usuario 

- Si tienes problemas de rendimiento, prueba a cambiar el valor predeterminado de Box64 a `Rendimiento` en la pestaña Configuración del contenedor -> Avanzado.
- Para las aplicaciones que usan .NET Framework, prueba a instalar `Wine Mono` que se encuentra en el Menú de inicio -> Herramientas del sistema.
- Si algunos juegos más antiguos no se abren, prueba a agregar la variable de entorno `MESA_EXTENSION_MAX_YEAR=2003` en la pestaña Configuración del contenedor -> Variables de entorno.
- Prueba a ejecutar los juegos usando el acceso directo en la pantalla de inicio de Winlator, allí puedes definir configuraciones individuales para cada juego.
- Para acelerar los instaladores, prueba a cambiar el valor predeterminado de Box64 a `Intermedio` en la pestaña Configuración del contenedor -> Avanzado.
- Para mejorar la estabilidad en los juegos que usan Unity Engine, prueba a cambiar el valor predeterminado de Box64 a `Estabilidad` o en la configuración del acceso directo agrega el argumento de ejecución `-force-gfx-direct`.

# Información 

This project has been in constant development since version 1.0, the current app source code is up to version 7.1, I do not update this repository frequently precisely to avoid unofficial releases before the official releases of Winlator.

# Credits and Third-party apps
- Ubuntu RootFs ([Focal Fossa](https://releases.ubuntu.com/focal))
- Wine ([winehq.org](https://www.winehq.org/))
- Box86/Box64 by [ptitseb](https://github.com/ptitSeb)
- PRoot ([proot-me.github.io](https://proot-me.github.io))
- Mesa (Turnip/Zink/VirGL) ([mesa3d.org](https://www.mesa3d.org))
- DXVK ([github.com/doitsujin/dxvk](https://github.com/doitsujin/dxvk))
- VKD3D ([gitlab.winehq.org/wine/vkd3d](https://gitlab.winehq.org/wine/vkd3d))
- D8VK ([github.com/AlpyneDreams/d8vk](https://github.com/AlpyneDreams/d8vk))
- CNC DDraw ([github.com/FunkyFr3sh/cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw))

Many thanks to [ptitSeb](https://github.com/ptitSeb) (Box86/Box64), [Danylo](https://blogs.igalia.com/dpiliaiev/tags/mesa/) (Turnip), [alexvorxx](https://github.com/alexvorxx) (Mods/Tips) and others.<br>
Thank you to all the people who believe in this project.
