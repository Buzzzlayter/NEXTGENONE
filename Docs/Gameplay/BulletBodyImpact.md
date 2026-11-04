# Реакция тела на пулю

Исправление падения при попадании в голову (2026-11-04): перед запуском drives и вызовом базового PhysicalAnimation Tick компонент получает bone-space pose через `GetBoneSpaceTransformsView` (завершает текущую parallel evaluation), затем проверяет размеры local/component-space буферов по ReferenceSkeleton. При недоступной позе новая физическая реакция пропускается, а уже активная продолжает таймер восстановления без обновления drives. Это защищает путь Tick от индексирования пустого массива трансформов; урон через GAS не зависит от наличия косметической реакции. Engine/ALS не изменены. Сборку и воспроизведение краша агент не запускал; исправление требует проверки пользователем.

UNGHitReactionComponent добавлен к существующему NextgenoneCharacter. Это компонент Physical Animation: он не создаёт вторую систему здоровья и не заменяет ALS locomotion/AnimBP.

## Попадание и урон

Movement Capsule игнорирует BallisticProjectile, а skeletal mesh блокирует этот объектный канал. Столкновение определяется по телам Physics Asset и возвращает BoneName; попадание в пустой участок капсулы больше не считается попаданием в тело. Для каждого персонажа нужен корректный Physics Asset. Сервер обновляет позу и кости даже вне экрана, чтобы коллайдеры соответствовали анимации; физика на dedicated server разрешена явно.

ANGBallisticProjectile при первом серверном столкновении применяет существующий GAS damage effect. Если урон успешно применён, вызывает физическую реакцию задетого персонажа. Импульс берётся из WeaponDefinition.BodyImpactImpulse, направление — из сегмента движения пули. Клиенты не выбирают урон, кость или силу. Проверки SourceLife/текущего ASC и защита от урона старому трупу сохранены. Реакция срабатывает и на смертельный выстрел, но автоматический death ragdoll этим изменением не добавляется. Уже мёртвые персонажи повторно не получают ни GAS-урон, ни эту реакцию.

## Физическая реакция

Сервер отправляет на компонент персонажа multicast: имя кости, точка в пространстве кости, направление и ограниченный импульс. На каждом участнике коротко включается симуляция задетой ветви скелета с Physical Animation drive и частичным blend, затем она плавно возвращается к анимации. RecoveryTime по умолчанию 0.35 с, PeakBlend 0.8. Повторные попадания обновляют время соответствующих тел; пересекающиеся ветви учитываются по отдельным physics bodies. До включения сохраняются blend weights, а симуляция, уже принадлежавшая другой системе, не отключается при восстановлении.

Корень и pelvis не переводятся в частичную физику: удар по pelvis передаётся на TorsoFallbackBone (по умолчанию spine_01). Это сохраняет владение движением за ALS/капсулой. Для уже активного ALS ragdoll компонент передаёт импульс телу без своих drives. При переходе в ragdoll активная реакция отпускает тела, сохраняя владение физикой за ALS. Точные Chaos-позы между клиентами не реплицируются; реплицируется косметическое событие, а урон остаётся серверным. Blueprint hook OnBulletBodyImpact доступен для звука/FX.

## Настройка

- В DA_Pistol / DA_Rifle / DA_Shotgun: BodyImpactImpulse (кг·см/с), отдельно от Damage. У дробовика значение применяется к каждой дробине.
- В Character → HitReaction: RecoveryTime, PeakBlend, Drive и TorsoFallbackBone.
- Для нового skeletal mesh проверить Physics Asset, тела и constraints; если имена костей отличаются, настроить TorsoFallbackBone. Кости без physics body не обеспечивают точный физический hit.

Цена решения: постоянное обновление skeletal pose на сервере и краткая симуляция ветвей при попаданиях. Нужна оценка на реальном числе игроков. Движение капсулы/массовое отталкивание игрока, dismemberment, blood decals, hit-zone damage multipliers и новая death-система не добавлены.

## Файлы

Новые: Source/NEXTGENONE/Gameplay/Character/NGHitReactionComponent.h/.cpp.
Изменены: NextgenoneCharacter.h/.cpp, NGCombat.h/.cpp, NGBallisticProjectile.h/.cpp, NGSetupCommandlet.cpp. Ассеты обновляются адресно через NGSetup -BodyImpactOnly с резервными копиями; оригинальный ALS plugin/AnimBP не редактируются.

PIE, игровые и сетевые тесты не запускались. Визуальная сила импульса, восстановление рук/ног/головы, одновременные дробины, ALS ragdoll/mantle, смерть/respawn, dedicated-server hit registration и задержки остаются непроверенными до отдельного апрува.
Результат: NEXTGENONEEditor Win64 Development — PASS, UBT exit 0, без диагностик компилятора. NGSetup -BodyImpactOnly — exit 0, 0 ошибок / 14 прежних предупреждений ALS/окружения. Адресно сохранены BP_NextgenoneCharacter, BP_BallisticProjectile, DA_Pistol, DA_Rifle, DA_Shotgun; наличие HitReaction и Character Physics Asset подтверждено при авторинге. BodyImpactImpulse: пистолет 1200, автомат 1800, дробовик 500 на дробину. NEXTGENONE.Build.cs дополнен PhysicsCore. Логи: Saved/Logs/NGBodyImpactBuild.log и NGBodyImpactAssets.log. Фактическая реакция в игре пока не проверялась.
