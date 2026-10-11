<!-- author: Claude · owner: Claude · 이 폴더의 문서는 Claude가 작성·관리한다 -->
# Claude 문서

이 폴더는 Claude(Anthropic)가 작성하고 관리하는 문서만 둔다. Codex가 관리하는 기준 문서 6종(GAME_DESIGN·PROJECT_PLAN·UI_README·TODO·MULTIPLAYER·HISTORY)과 위임 기록(EXECUTION_DECISIONS)은 `Docs/Codex/`에 있으며 이 폴더와 섞지 않는다. 소유권과 수정 범위는 [AGENTS.md의 작성자별 문서 분리](../../AGENTS.md#작성자별-문서-분리)를 따른다.

## 1 식별 규칙

| 구분 | Claude | Codex |
| --- | --- | --- |
| 위치 | `Docs/Claude/`, 루트 `CLAUDE.md` | `Docs/Codex/*.md`·`Docs/Codex/History/`, 루트 `README.md` (`AGENTS.md`는 공용) |
| 문서 첫 줄 | `<!-- author: Claude ... -->` | 표식 없음(기존 형식 유지) |
| 제안 번호 | `Claude 제안 1`, `Claude 제안 2` … | `제안 1`, `제안 2` … |
| 커밋 제목 | `[claude] ` | `[codex] ` |

## 2 문서 목록

| 문서 | 내용 |
| --- | --- |
| [REVIEW_2026-10-11](REVIEW_2026-10-11.md) | 2026-10-11 전체 코드·구조 리뷰 결과 |
| [PROPOSALS](PROPOSALS.md) | 사용자 선택이 필요한 Claude 제안. 선택 체크는 사용자가 한다. |
| [HANDOFF](HANDOFF.md) | Claude가 직접 컴파일·커밋하지 못한 변경의 인계 기록 |
