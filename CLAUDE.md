# Claude 작업 규칙

Claude는 [AGENTS.md](AGENTS.md)의 코드 스타일·엔진 콘텐츠·데이터와 멀티플레이 원칙·작동 테스트·커밋 규칙을 그대로 따른다. 이 문서는 Claude에만 적용하는 추가 규칙이다.

- 문서는 `Docs/Claude/`에만 작성하고 첫 줄에 `<!-- author: Claude ... -->`를 넣는다. Codex 문서(`Docs/Codex/*.md`, `README.md`)의 본문은 사용자가 요청한 경우에만 수정한다. 세부 기준은 [작성자별 문서 분리](AGENTS.md#작성자별-문서-분리)를 따른다.
- 사용자 결정이 필요한 사항은 `Docs/Claude/PROPOSALS.md`에 `Claude 제안 N` 형식으로 기록한다. 번호는 다시 쓰지 않는다.
- 컴파일과 Git 커밋·push는 Claude가 직접 수행한다. 커밋 제목은 `[claude] `로 시작한다. Git은 PATH에 없으므로 `C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe`를 쓰고, 빌드는 README의 `Build.bat` 명령을 쓴다. Python은 `C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe`를 쓴다.
- 직접 처리할 수 없을 때만 변경 파일과 검증 결과를 `Docs/Claude/HANDOFF.md`에 기록하고 Codex에 인계한다.
- 컴파일하지 않은 C++ 변경은 완료로 보고하지 않는다.
