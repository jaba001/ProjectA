# T14 · 온라인 인증·중앙 저장·MMR 연동 준비

[작업 대기열](T14_QUEUE.md) · [협동 기획](T14_COOP_DESIGN.md) · [승계 구현 메모](T14_RESUME.md) · [네트워크 검증](T14_NETWORK.md)

## 현재 상태 — 2026-09-10

7번 독립 범위 `152aed7`의 검증·커밋·push를 확인한 뒤 8번 준비를 진행했다. 이 문서는 현재 코드 조사와 서비스 선택 후 구현할 경계를 기록한다. 실제 온라인 인증·중앙 저장·MMR 코드는 아직 연결하지 않았으며 8번은 미완료다.

`ProjectA.uproject`, `ProjectA.Build.cs`, `Config`에는 프로젝트의 EOS/Steam/PlayFab 활성화·연동 설정이 없다. Runtime에는 공급자 로그인, 서버 티켓 검증, 온라인 세션 생성과 Backend 요청이 없다. 엔진의 기본 플러그인이나 `ERunIdentityOrigin::AccountProvider` 값은 실제 인증 성공을 뜻하지 않는다. 현재 `AssignRunParticipant`는 신뢰된 서버 C++가 테스트 계정을 알려진 연결에 배정하는 지점이다.

6번의 `FLocalRunAuthorityStore`는 같은 PC의 개발 저장소 대역이며 관리 Run v4의 메뉴·승계·전투 복구와 연결한다. 이를 온라인 중앙 저장으로 노출하지 않는다. v4는 고정된 C++ 개발 호출자만 허용하고 AccountProvider 생성·재개를 거절한다. 기존 Run v1/v2/v3와 Snapshot v1은 현재 로컬 계약을 유지하며 실제 계정·온라인 전송·공유 저장은 8번에서 별도 연결한다.

## 선택한 방향과 비용 조건

사용자는 후속 다섯 항목의 **4B(Steam + PlayFab)**를 선택했고, Steam 우선 및 서버 운영비를 피하고 싶다는 조건을 추가했다. 전투는 Unreal Listen Server가 실행하고 Steam P2P/SDR로 연결하는 방향이다. 별도 전투 서버를 임대하지 않아도 되지만 실제 4인 인터넷 접속의 Host CPU·네트워크·복구 동작은 별도 검증해야 한다. [Steam SDR](https://partner.steamgames.com/doc/features/multiplayer/steamdatagramrelay?l=english)

P2P는 전투 전송을 해결한다. 공동 Run의 최신 저장, 동시에 둘 이상 이어가지 못하게 하는 승계 승인, 결과 검증과 MMR은 별도 책임이다. Steam Cloud의 기본 저장은 같은 사용자의 여러 PC 사이 동기화이며 사용자별 파일을 격리한다. 참가자에게 체크포인트를 복사해 주더라도 각자가 별도로 재개하는 진행 분기를 막는 중앙 비교·갱신 기능은 자동 제공되지 않는다. [Steam Cloud](https://partner.steamgames.com/doc/features/cloud?l=english)

Steam Leaderboards는 점수 저장·표시 기능이며 ProjectA 전투 결과의 정당성을 검사하지 않는다. Trusted 쓰기를 켜면 클라이언트 쓰기를 막지만 점수 제출은 publisher key를 사용하는 보안 서버에서 해야 한다. 그 키를 Listen Host 클라이언트에 넣지 않는다. [Leaderboard 설정](https://partner.steamgames.com/doc/features/leaderboards?l=english), [SetLeaderboardScore](https://partner.steamgames.com/doc/webapi/ISteamLeaderboards#SetLeaderboardScore)

2026-09-10 확인한 PlayFab Development Mode 문서는 개발 타이틀 최대 10개, 타이틀당 누적 생성 계정 1,000개와 서비스별 한도를 안내한다. 이는 출시 후 영구 무료 운영 보장이 아니다. Foundation Mode는 Xbox 출시 또는 출시 계획과 Partner Center 연결 조건이 있어 Steam 전용 프로젝트가 자동으로 해당되는 것으로 가정하지 않는다. Azure Functions의 실제 계산 비용도 별도다. 일부 가격 페이지의 과거 100K 문구보다 현재 Title 모드·개발 한도를 확인한다. [Development Mode](https://learn.microsoft.com/en-us/gaming/playfab/pricing/development-mode), [서비스 모드](https://learn.microsoft.com/en-us/gaming/playfab/get-started/mode-overview), [가격](https://developer.microsoft.com/en-us/games/products/playfab/pricing/)

따라서 Steam P2P와 무료 개발 한도를 우선 활용하되 유료 리소스·과금 전환은 임의로 활성화하지 않는다. 출시 시점에도 자체 Backend 비용을 전혀 허용하지 않는다면 현재 요구한 신뢰할 수 있는 단일 진행·MMR을 같은 수준으로 유지할 방법을 추가 결정해야 한다. 이번 답변을 랭크 제거·무검증 점수 허용·오프라인 분기 승인으로 해석하지 않는다.

## 이전 서비스 비교

두 질문은 독립적인 선택이다. 추천 표시나 답변 대기는 승인으로 간주하지 않으며, 상태 변화 없이 질문을 반복하지 않는다.

| 질문 | 선택 | 장점 | 부담과 준비 |
|---|---|---|---|
| 로그인·초대·P2P | A · EOS 우선 | Epic의 공식 UE 플러그인과 여러 스토어를 고려한 확장 | Developer Portal 제품·Sandbox·Deployment·Client 정책, 테스트 계정과 로그인 방식 준비 |
| 로그인·초대·P2P | B · Steam 우선 | Steam 계정·친구·초대·앱 소유권 흐름에 집중 | Steamworks 프로젝트/AppID·접근 가능한 테스트 계정, 다른 플랫폼 지원은 추가 준비 |
| Backend | A · PlayFab 관리형 | 공식 Unreal SDK와 계정·게임 데이터·서버 기능 활용, 일부 운영 부담 감소 | PlayFab Title, ProjectA용 서버 로직과 원자적 저장 설계, 사용량·제품 의존성 관리 |
| Backend | B · 직접 HTTPS 서버 + PostgreSQL | 승계·revision·참여 이력·결과 중복 방지를 필요한 트랜잭션으로 구성 | 서버 배포·TLS·인증 검증·DB·백업·운영 직접 관리 |

위 표는 이전의 독립 질문별 보기다. 후속 통합 보기 4B는 Steam+PlayFab이며 위의 비용 조건과 함께 채택했다. 앱 식별 정보와 권한 있는 무료 개발 환경이 정해진 뒤 필요한 설정을 연결하며, 서버 비밀 키를 저장소나 클라이언트에 넣지 않는다.

EOS는 제품 등록과 OSS EOS 구성이 필요하고, UE에 포함된 EOS SDK를 사용할 수 있다. Steam은 OSS Steam과 Steamworks 설정을 사용한다. [UE 5.7 OSS EOS](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-eos-plugin-in-unreal-engine?application_version=5.7), [UE 5.7 OSS Steam](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-steam-interface-in-unreal-engine?application_version=5.7)

PlayFab은 공식 Unreal SDK와 CloudScript를 제공하지만 ProjectA의 승계·MMR 트랜잭션을 자동으로 구현해 주지는 않는다. Azure Functions를 고르면 별도의 Azure 준비도 필요하다. [Unreal SDK](https://learn.microsoft.com/en-us/xbox/playfab/sdks/unified-unreal/overview), [CloudScript](https://learn.microsoft.com/en-us/xbox/playfab/live-service-management/service-gateway/automation/cloudscript/), [Azure Functions](https://learn.microsoft.com/en-us/xbox/playfab/live-service-management/service-gateway/automation/cloudscript-af/)

직접 서버를 선택하면 PostgreSQL의 조건부 갱신과 트랜잭션을 이용해 Run별 동시 변경을 처리할 수 있다. 결과·MMR의 논리적 한 번 반영은 해당 트랜잭션과 고유 키를 함께 설계해야 한다. [PostgreSQL 동시 갱신과 격리](https://www.postgresql.org/docs/current/transaction-iso.html)

## Unreal 확장 지점

공급자 SDK를 직접 감싸는 별도 네트워크 계층보다 기존 Online Subsystem과 엔진 수명 주기를 우선한다. 새 Online Services API는 5.7 공식 문서에서 Beta이므로, 이번 준비에서는 OSS를 우선 검토한다. [UE 5.7 Online Services](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-services-in-unreal-engine?application_version=5.7)

로컬 UE 5.7에서 `OnlineSubsystemEOS`, `OnlineSubsystemSteam`, `SteamSockets` 플러그인을 확인했다. EOS의 현재 NetDriver는 `SocketSubsystemEOS`의 `UNetDriverEOS`이며 `UNetDriverEOSBase`와 `bIsUsingP2PSockets`는 5.6부터 deprecated다. SteamSockets는 `USteamSocketsNetDriver`를 사용한다. 기존 SteamNetDriver도 현재 `SocketSubsystemSteamIP` 모듈에 있으므로 과거 문서의 설정 문자열을 그대로 복사하지 않고 선택한 플러그인의 로컬 코드와 대조한다.

| 확장 지점 | 연결할 책임 |
|---|---|
| Online Subsystem Identity·Session과 공급자 NetDriver | 로그인/로그아웃·초대·세션 생성·접속, 선택한 P2P transport |
| GameInstance/LocalPlayer Subsystem과 엔진 비동기 delegate | 메뉴·레벨 이동 중 로컬 로그인 수명 관리, 이전 연결의 늦은 완료 무효화 |
| GameMode 로그인 수명 주기·PlayerState | 서버가 검증한 계정을 실제 연결에 대응, 원래 참가자·중복 연결·Host·정원 확인 |
| PlayerController Server RPC | 검증된 소유 연결의 값 명령을 현재 서버 권위 검사로 전달 |
| 공식 Backend SDK 또는 Unreal HTTP·JSON | 서버 인증 결과·Run 기준 기록·승계 승인·결과 조회/제출 |
| USTRUCT·USaveGame·기존 Run/Checkpoint 검증 | 값 데이터와 로컬 호환 유지, 내려받은 데이터의 버전·크기·카탈로그·수치 검증 |

로컬 로그인 성공만으로 다른 클라이언트가 제출한 계정을 믿지 않는다. 예를 들어 Steam 티켓은 보안 서버에서 검증한 계정에 연결해야 한다. 표시 이름·PIE PlayerId·요청 본문의 계정·클라이언트의 Verified 플래그를 인증 증거로 사용하지 않는다. [Steam 사용자 인증과 소유권](https://partner.steamgames.com/doc/features/auth?l=english)

## 중앙 Run·결과 계약의 요구사항

아래는 기존 기획에서 도출한 요구사항이며 미구현이다. 필드 이름과 온라인 전송 포맷, 공급자별 저장 구현을 확정하는 명세는 아니다.

1. 인증한 원래 참가자만 Run의 최신 기록을 읽거나 승인된 동작을 요청한다. Host 승계는 캐릭터 소유권을 바꾸지 않는다.
2. Run별 revision·HostEpoch·실행 세대와 전체 확정 본문을 함께 갱신한다. 정확한 이전 값으로 비교한 한 요청만 성공하며 충돌 후 로컬 파일 저장으로 우회하지 않는다. 승인된 후임자는 원래 Host의 로컬 파일 없이 최신 본문을 얻을 수 있어야 한다.
3. 요청 식별자로 재시도 결과를 조회한다. 같은 요청의 같은 본문은 기존 결과를 반환하고, 같은 식별자에 다른 본문은 거절한다. 타임아웃은 서버 미반영의 증명이 아니므로 새 식별자로 같은 행동을 다시 확정하지 않는다.
4. 같은 전투 시도의 결과·MMR을 논리적으로 한 번만 반영한다. 다른 요청 식별자를 붙인 중복 제출도 막고, 저장 성공/응답 유실 뒤 재시도 결과를 확인한다.
5. MMR의 인간 참가자는 서버가 승인한 참여 이력과 확정할 판정 시점으로 계산한다. 제출자가 보낸 인간 목록을 그대로 믿지 않으며 불참자의 추가 MMR 변동을 만들지 않는다.
6. Listen Host의 결과 주장을 경쟁 서버가 어떤 증거로 검증할지 먼저 결정한다. 서명이나 전송 보안만으로 정당한 전투 결과를 증명한 것으로 취급하지 않는다. 검증 방식이 미정이면 랭크 반영을 열지 않는다.
7. 구성 없음·인증 실패/만료·접근 거절·오래된 세대·정책 미정·일시 장애를 구분해 처리한다. 온라인 실패를 개발용 계정이나 로컬 저장 성공으로 바꾸지 않는다. 이전 연결/요청의 늦은 성공도 현재 Run을 변경하지 못한다.

## 선택 후 구현·검증 순서

| 순서 | 구현 | 실제 수용 검증 |
|---|---|---|
| 1 | 선택한 개발 서비스와 UE 플러그인 설정 | 서로 다른 두 계정의 로그인·만료/취소/실패, 개발용 계정의 온라인 승격 거절 |
| 2 | 서버 티켓 검증과 원래 참가자 연결 | 타인 계정 사칭·비참가자·중복 연결 거절, 새 연결에 이전 인증 완료 적용 거절 |
| 3 | 초대·세션·P2P 접속 | 서로 다른 프로세스/PC의 2인 연결 후 3·4인 소유권·전투 상태 검증 |
| 4 | 중앙 Run 기준 기록과 승인된 재개 | 동시 갱신·승계 충돌·오래된 세대·응답 유실 재조회, 원래 Host 없이 최신 본문 복원 |
| 5 | 경쟁 결과 검증과 참여 이력 | 변조/중복/다른 요청 ID 재제출 거절, 선택한 정책에 따른 인간 참가자만 판정 |
| 6 | MMR 계산·원자적 반영 | 동일 시도 한 번 반영, 장애/재시도에서도 중복 증감 없음, 불참자 추가 변동 없음 |

4번의 실제 승계는 6번 정책·구현이 필요하다. MMR 판정 시점·계산/정산 단위·이탈/턴 반복 악용 대응·전투 결과 검증 방식은 아직 확정하지 않았다. 플랫폼/Backend 선택만으로 이 정책들을 함께 승인한 것으로 간주하지 않는다. 결정이 필요한 단계에 도달하면 구체적인 보기와 영향을 추가 확인한다.

## 최초 8번 준비의 검증 기록

최초 준비에서 프로젝트와 설치된 UE 5.7 소스, 위 공식 문서를 대조했다. 당시 문서 10개의 로컬 링크 133개와 `git diff --check`를 통과했고 문서만 변경해 C++ 빌드·PIE는 반복하지 않았다. `f877e4d`로 커밋·push했으며 플러그인 활성화·로그인·서비스 생성·유료 리소스·Backend 배포·MMR 반영은 실행하지 않았다. 앞선 7번의 빌드/65건/AI 3건 성공을 8번 온라인 통합 검증으로 계산하지 않는다. 후속 확정 정책 반영과 코드 검증은 [대기열](T14_QUEUE.md)에 별도로 기록한다.

Steam 우선과 4B에 대한 답변은 받았다. 실제 개발 서비스의 준비 상태·무료 한도와 아직 미정인 MMR 정책은 남아 있다. 공급자 선택을 다시 묻지 않으며 답변이나 외부 상태 변화가 없으면 동일 질문·조사·준비 커밋을 반복하지 않는다.
