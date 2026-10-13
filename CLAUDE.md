# Claude 작업 규칙

Claude는 [AGENTS.md](AGENTS.md)의 모든 규칙을 그대로 따른다. 문서는 Codex와 함께 `Docs/`에서 관리한다([공동 작업](AGENTS.md#공동-작업)). 이 문서는 Claude에만 적용하는 추가 규칙이다.

- 컴파일과 Git 커밋·push는 Claude가 직접 수행한다. 커밋 제목은 `[claude] `로 시작한다. Git은 PATH에 없으므로 `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe`를 쓰고, 빌드는 README의 `Build.bat` 명령을 쓴다. Python은 `C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe`를 쓴다.
- 컴파일하지 않은 C++ 변경은 완료로 보고하지 않는다.
