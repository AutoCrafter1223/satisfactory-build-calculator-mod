# Satisfactory Build Calculator 1.0.1

## 한국어 변경사항

- 생산 계산 결과를 아이콘이 포함된 연결형 카드로 개선했습니다.
- 카드 선택, 접기/펴기, 상세/간소화 보기의 배치와 가독성을 개선했습니다.
- 생산량 단위, 건물 필요 수량, 설치 수량 및 소비 전력 표시를 정리했습니다.
- 현재 세이브에서 해금된 제조법만 보거나 모든 제조법을 선택할 수 있는 전환 버튼을 추가했습니다.
- 계산기에서 선택한 카드와 모든 하위 생산시설을 건설 목표로 안전하게 추가하도록 보완했습니다.
- 건설 목표에 부모-자식 연결선, 아이템 아이콘 및 더 밝은 선택 표시를 추가했습니다.
- 완료된 목표는 목록에서 숨기고, 목표가 비어 있을 때 불필요한 갱신을 줄였습니다.
- 목표와 건물 추적 데이터를 휘발성 세션 데이터로 변경하여 세이브 파일에 모드 목표 데이터를 기록하지 않습니다.
- 한국어, 영어, 중국어 간체 및 독일어 UI 번역과 언어 전환 반영을 개선했습니다.
- 영어 카드의 잘못된 `installed` 표기를 `build`로 수정했습니다.

주의: 아직 충분한 실제 플레이 및 멀티플레이 테스트가 완료되지 않은 버전입니다. 중요한 세이브는 별도로 백업한 뒤 사용해 주세요.

## English changelog

- Improved the production graph with connected cards and item icons.
- Refined card selection, expand/collapse controls, and detailed/compact layouts for better readability.
- Clarified rate units, required machine counts, rounded build counts, and power usage.
- Added a toggle between unlocked recipes only and all recipes.
- Made selected-branch goal creation validate and add every descendant production building as one safe batch.
- Added parent-child guides, item icons, and a brighter selection highlight to construction goals.
- Completed goals are hidden, and unnecessary refresh work is skipped while the goal list is empty.
- Construction goals and building tracking now remain volatile session data and do not write mod goal data into save files.
- Improved Korean, English, Simplified Chinese, and German UI localization and live language updates.
- Replaced the misleading English `installed` card label with `build`.

Warning: This version has not yet completed extensive real-world or multiplayer testing. Back up important saves before use.
