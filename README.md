GAS 기반 Turn-Based RPG Prototype (For The King Inspired)
Unreal Engine 5와 Gameplay Ability System(GAS)을 활용하여
턴 기반 RPG 전투 시스템을 설계 및 구현하는 프로젝트입니다.

For The King 스타일의 파티 기반 턴제 구조를 참고하여
Unit, Ability, Effect, Attribute 중심의 확장 가능한 전투 시스템 아키텍처를 구현하는 것을 목표로 합니다.

본 프로젝트는 단순한 기능 구현보다 GAS 기반 시스템 설계와 구조 이해에 중점을 둔 개인 연구 프로젝트입니다.

Project Overview
이 프로젝트는 Unreal Engine의 Gameplay Ability System(GAS) 을 활용하여
턴 기반 RPG 전투 시스템을 구현하는 것을 목표로 합니다.

GAS는 MMO 및 대규모 RPG에서 사용되는 확장 가능한 게임플레이 시스템 프레임워크로,

Ability 기반 행동 정의

Effect 기반 결과 처리

Attribute 기반 수치 관리

와 같은 구조를 통해 복잡한 전투 시스템을 구조적으로 구현할 수 있습니다.

본 프로젝트에서는 GAS 구조를 기반으로
턴 기반 RPG 전투 시스템에 적합한 아키텍처를 설계하였습니다.

Goals
Unreal Engine Gameplay Ability System 구조 이해

GAS 기반 Ability / Effect / Attribute 구조 구현

턴 기반 RPG 전투 시스템 아키텍처 설계

확장 가능한 전투 시스템 구조 설계

Unreal Engine C++ 기반 게임 시스템 구현

Tech Stack
Engine

Unreal Engine 5

Language

C++

Core Systems

Gameplay Ability System (GAS)

Turn-Based Combat Architecture

Unit System

Tools

Visual Studio

Git

GitHub

Core Architecture
본 프로젝트의 전투 시스템은 GAS 구조를 기반으로 설계되었습니다.

Unit (Actor)
 ├ AbilitySystemComponent
 ├ AttributeSet
 └ GameplayAbilities

GameplayAbility
 └ 캐릭터 행동 정의
    (Attack / Skill / etc)

GameplayEffect
 └ Ability 결과 처리
    (Damage / Buff / Debuff)

AttributeSet
 └ 캐릭터 수치 관리
    (HP / Attack / Defense)
이 구조를 통해 행동(Ability)과 결과(Effect)를 분리하여
확장 가능한 전투 시스템을 구성할 수 있습니다.

Unit Architecture
게임 내 전투 유닛은 다음과 같은 구조로 구성됩니다.

UnitBase
 ├ PlayerUnit
 └ EnemyUnit
UnitBase
모든 전투 유닛의 공통 기능을 담당하는 베이스 클래스입니다.

주요 역할

AbilitySystemComponent 보유

AttributeSet 관리

기본 전투 인터페이스 제공

PlayerUnit
플레이어 캐릭터 유닛 클래스입니다.

플레이어 Ability 사용

Player Controller와 연동

EnemyUnit
적 캐릭터 유닛 클래스입니다.

AI 기반 Ability 사용

전투 로직 처리

Controller Architecture
PartyPlayerController
파티 기반 턴제 게임 구조를 고려하여
플레이어 유닛을 제어하는 Party Controller 구조를 사용합니다.

향후 전투 시스템 확장을 위해 다음 구조를 계획하고 있습니다.

CombatManager
TurnManager
AIController
Gameplay Ability Example
기본 공격 Ability는 GAS 구조를 기반으로 구현되었습니다.

Ability Activate
      ↓
GameplayEffect Apply
      ↓
AttributeSet Damage 적용
      ↓
HP 감소
이 구조를 통해 Ability 로직과 실제 효과 처리를 분리할 수 있습니다.

Project Structure
Source/ProjectA

Unit
 ├ UnitBase
 ├ PlayerUnit
 └ EnemyUnit

Controller
 └ PartyPlayerController

GAS
 ├ Ability
 ├ Effect
 └ Attribute
Key Implementation Points
본 프로젝트에서 중점적으로 구현한 요소는 다음과 같습니다.

GAS 기반 Ability 시스템 구현
GameplayAbility 기반 행동 정의

Ability 실행 시 GameplayEffect 적용

Attribute 기반 캐릭터 능력치 관리
AttributeSet을 통해 HP 및 전투 수치 관리

Unit 기반 전투 구조 설계
UnitBase 기반 상속 구조

Player / Enemy 분리 설계

확장 가능한 전투 아키텍처
Ability / Effect / Attribute 분리 구조

향후 Skill, Buff, Debuff 시스템 확장 가능

Future Work
현재 프로젝트는 기본 전투 시스템 구조 구현 단계이며
향후 다음 기능을 추가할 계획입니다.

Combat Manager 시스템 구현

Turn Manager 구현

Grid 기반 이동 시스템

AI 행동 로직 구현

스킬 Ability 확장

상태이상 Effect 시스템 구현

Project Purpose
본 프로젝트는 Unreal Engine Gameplay Ability System을 활용한
확장 가능한 RPG 전투 시스템 아키텍처 설계 경험을 쌓기 위한 개인 프로젝트입니다.

GAS 기반 시스템 구조를 이해하고
실제 게임 개발에서 활용 가능한 전투 시스템 구조를 연구하는 것을 목표로 합니다.
