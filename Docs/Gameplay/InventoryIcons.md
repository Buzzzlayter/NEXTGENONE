# Иконки и новые предметы — 2026-11-07

Восемь Texture2D перенесены из пользовательского проекта VisualSandbox с сохранением путей `/Game/VisualSandbox/2DAssets/`. Исходный проект не изменён.

| Definition в /Game/Gameplay/Inventory | Текстура относительно /Game/VisualSandbox/2DAssets |
|---|---|
| ID_Pistol | WeaponCards/T_Lyra_Pistol |
| ID_Rifle | WeaponCards/T_Lyra_Rifle |
| ID_Shotgun | WeaponCards/T_Lyra_Shotgun |
| ID_Ammo_Pistol | Items/T_Round_9mm |
| ID_Ammo_Rifle | Items/T_Round_7_62 |
| ID_Ammo_Shotgun | Items/T_Round_12_Gauge |
| ID_Medkit | Items/T_IFAK |
| ID_Money | Items/T_Mone_Icon |

ID_Medkit и ID_Money — Blueprint-наследники общего NGInventoryItemDefinition, без equipment/stat fragments. MaxStackSize: аптечка 10, деньги 10000; значения редактируются в defaults. Деньги представлены обычным переносимым loot-стеком; применение аптечки как GAS-способности в этот пакет не входит.

Для выдачи добавить нужный класс в PlayerState.Inventory.InitialContents либо InitialContents коробки. Для предмета в мире использовать общий BP_ItemPickup с соответствующим ItemClass. Стартовые списки, карта и пользовательский виджет слота этим пакетом не изменялись.

Старые шесть T_UI_Icon_RangedWeapons_* из /Game/UI/Hud/Art удалены после сохранения новых ссылок и проверки referencers. Backup: Saved/InventoryIconReplacement/20261107. Editor read-back подтвердил icon/displayName/MaxStackSize; новые текстуры ссылаются только на /Script/InterchangeEngine, других игровых зависимостей нет. Игровые тесты и сборка не запускались.
