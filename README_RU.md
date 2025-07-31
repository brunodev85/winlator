[English](README.md) | [Русский](README_RU.md)

<p align="center">
	<img src="logo.png" width="376" height="128" alt="Winlator Logo" />
</p>

# Winlator

Winlator — это приложение для Android, которое позволяет запускать Windows-приложения (x86_64) с помощью Wine и Box86/Box64.

# Установка

1. Скачайте и установите APK-файл (Winlator_10.1.apk) с [GitHub Releases](https://github.com/brunodev85/winlator/releases)
2. Запустите приложение и дождитесь окончания процесса установки

----

[![Запуск на Youtube](https://img.youtube.com/vi/ETYDgKz4jBQ/3.jpg)](https://www.youtube.com/watch?v=ETYDgKz4jBQ)
[![Запуск на Youtube](https://img.youtube.com/vi/9E4wnKf2OsI/2.jpg)](https://www.youtube.com/watch?v=9E4wnKf2OsI)
[![Запуск на Youtube](https://img.youtube.com/vi/czEn4uT3Ja8/2.jpg)](https://www.youtube.com/watch?v=czEn4uT3Ja8)
[![Запуск на Youtube](https://img.youtube.com/vi/eD36nxfT_Z0/2.jpg)](https://www.youtube.com/watch?v=eD36nxfT_Z0)

----

# Полезные советы

- Если у вас проблемы с производительностью, попробуйте в настройках контейнера в разделе «Дополнительно» изменить предустановку Box64 на «Производительность».
- Для приложений, использующих .NET Framework, попробуйте установить `Wine Mono` через Меню Пуск -> Системные инструменты -> Установщики.
- Если некоторые старые игры не запускаются, попробуйте добавить переменную окружения `MESA_EXTENSION_MAX_YEAR=2003` в настройках контейнера -> Переменные окружения.
- Для запуска игр используйте ярлыки на главном экране Winlator, где можно задать индивидуальные настройки для каждой игры.
- Чтобы правильно отображать игры с низким разрешением, попробуйте включить опцию «Принудительный полноэкранный режим» в настройках ярлыка.
- Для улучшения стабильности в играх на движке Unity попробуйте изменить предустановку Box64 на «Стабильность» или в аргументах запуска ярлыка добавить `-force-gfx-direct`.

# Информация

Проект находится в активной разработке с версии 1.0, текущий исходный код приложения — версия 7.1. Репозиторий обновляется нечасто, чтобы избежать неофициальных релизов до официальных выпусков Winlator.

# Благодарности и сторонние проекты

- Патчи GLIBC от [Termux Pacman](https://github.com/termux-pacman/glibc-packages)
- Wine ([winehq.org](https://www.winehq.org/))
- Box86/Box64 от [ptitseb](https://github.com/ptitSeb)
- Mesa (Turnip/Zink/VirGL) ([mesa3d.org](https://www.mesa3d.org))
- DXVK ([github.com/doitsujin/dxvk](https://github.com/doitsujin/dxvk))
- VKD3D ([gitlab.winehq.org/wine/vkd3d](https://gitlab.winehq.org/wine/vkd3d))
- CNC DDraw ([github.com/FunkyFr3sh/cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw))

Особая благодарность всем разработчикам, участвовавшим в этих проектах.<br>
Спасибо всем, кто верит в этот проект.
