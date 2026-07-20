# Zelda3 — Port Dual Screen, Multi-Idioma & Nintendo Switch

Fork do projeto original com suporte a tela dividida, múltiplos idiomas e port para Nintendo Switch.

### Créditos

Este projeto é baseado em:

- **[snesrev/zelda3](https://github.com/snesrev/zelda3)** — Reimplementação original em C do Zelda 3: A Link to the Past. Engine principal, renderer, emulação SNES PPU/DSP, extração de assets e toda lógica de gameplay.
- **[samyost1/zelda3-android](https://github.com/samyost1/zelda3-android)** (branch `dual-screen`) — Port Android que introduziu o conceito de tela dividida: segunda tela com mapa de masmorra, mapa de overworld, inventário, equipamento e configurações — tudo renderizado a partir do estado do jogo via SDL.

---

## Sobre

Reimplementação reversa do Zelda 3 - A Link to the Past, com aproximadamente 70-80k linhas de código C. O jogo é jogável do início ao fim.

É necessário possuir uma cópia do ROM para extrair os recursos do jogo (níveis, imagens). Uma vez extraído, o ROM não é mais necessário.

Utiliza a implementação PPU e DSP do [LakeSnes](https://github.com/elzo-d/LakeSnes), com muitas otimizações de performance.

---

## Funcionalidades adicionais (vs projeto original)

| Funcionalidade | Original | Este fork |
|---------------|----------|-----------|
| Tela dividida (dual screen) | Não | Sim — horizontal, vertical e TATE |
| Segunda tela interativa | Não | Sim — mapa, itens, equipamento, configurações |
| Nintendo Switch (NRO) | Não | Sim — homebrew via Atmosphere |
| Modo TATE (Flip Grip) | Não | Sim — segunda tela preenche toda a área visível |
| Múltiplos idiomas (PT, ES, DE, FR, JA, ZH, KO) | Não | Sim — language pack overlay |
| Widescreen 16:9 | Não | Sim |
| Save/Load com slots (L3+X/Y) | Não | Sim — 10 slots (0-9) |
| Notificações visuais (salvar/carregar) | Não | Sim |
| Touch screen (Switch) | Não | Sim — nos modos horizontal e TATE |
| Pixel shaders | Sim | Sim |
| Áudio MSU | Sim | Sim |
| Item secundário (botão X) | Sim | Sim |
| Trocar item com L/R | Sim | Sim |

---

## Nintendo Switch

### Pré-requisitos

- [DevKitPro](https://devkitpro.org/wiki/Getting_Started)
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere)

### Compilação

```sh
(dkp-)pacman -S git switch-dev switch-sdl2 switch-tools
cd platform/switch
make
```

### Arquivos no SD card

Copie todos os arquivos para a mesma pasta (ex: `/switch/zelda3/`):

```
zelda3.nro
zelda3_assets.dat
zelda3.ini
zelda3.sfc
zelda3_langpack_pt.dat   (opcional, para Português)
```

### Controles do Switch

#### Controles gerais

| Botão | Ação |
|-------|------|
| D-Pad | Mover Link |
| A | Ação / Confirmar |
| B | Cancelar / Correr |
| X | Item secundário |
| Y | Item equipado no anel Y |
| L1 | Pausar |
| R1 | Pausar |
| Start | Menu de pausa |
| Select | Selecionar |
| L / R | Trocar item rápido |
| Minus (-) | Salvar (slot 0 por padrão) |
| Plus (+) | Carregar (slot 0 por padrão) |

#### Modo de tela (R3)

| Botão | Ação |
|-------|------|
| R3 (clique do stick direito) | Alternar modo de tela |

**Modos disponíveis:**

1. **1 tela** — Jogo em tela cheia, segunda tela oculta
2. **Horizontal** — Jogo à esquerda (70%), segunda tela à direita (30%) em proporção 4:3, com suporte a toque
3. **Vertical (TATE)** — Modo Flip Grip para jogo em retrato:
   - Jogo e segunda tela renderizados lado a lado, ambos rotacionados 270° CW
   - Segunda tela esticada para preencher toda a área visível (sem barras pretas)
   - Suporte a toque com mapeamento de rotação inversa

#### Save/Load com slots

| Botão | Ação |
|-------|------|
| L3 (segurar) | Abrir seletor de slots (0-9) |
| L3 + D-Pad Cima/Baixo ou L/R | Navegar entre slots |
| L3 + X | Salvar no slot selecionado |
| L3 + Y | Carregar do slot selecionado |
| Minus (-) | Salvar rápido no slot 0 |
| Plus (+) | Carregar rápido do slot 0 |

O slot selecionado é salvo no `zelda3.ini` como `SaveSlot`.

---

## Teclado (Desktop)

| Tecla | Ação |
|-------|------|
| Setas | Mover |
| Enter | Start |
| Shift Direito | Select |
| X | A |
| Z | B |
| S | X |
| A | Y |
| C | L |
| V | R |
| Tab | Modo turbo |
| W | Preencher vida/magia |
| Shift+W | Preencher rupias/bombas/flechas |
| Ctrl+E | Reiniciar |
| P | Pausar (com escurecimento) |
| Shift+P | Pausar (sem escurecimento) |
| Ctrl+Up | Aumentar janela |
| Ctrl+Down | Diminuir janela |
| T | Alternar replay turbo |
| O | Setar chave de masmorra para 1 |
| K | Limpar histórico de input do joypad |
| L | Parar replay de snapshot |
| R | Alternar renderer rápido/lento |
| F | Mostrar performance do renderer |
| F1-F10 | Carregar snapshot |
| Shift+F1-F10 | Salvar snapshot |
| Ctrl+F1-F10 | Replay do snapshot |
| Alt+Enter | Tela cheia |
| 1-9 | Carregar snapshot de masmorra |
| Ctrl+1-9 | Rodar replay de masmorra em turbo |

---

## Multi-idioma

### Configuração

No `zelda3.ini`, defina:

```ini
[General]
Language=pt
```

### Idiomas suportados

| Código | Idioma |
|--------|--------|
| `us` | Inglês (padrão, sem language pack) |
| `pt` | Português |
| `es` | Espanhol |
| `de` | Alemão |
| `fr` | Francês |
| `ja` | Japonês |
| `zh` | Chinês |
| `ko` | Coreano |

### Gerar language pack

```sh
python3 assets/extract_langpack.py pt zelda3_pt.sfc zelda3_langpack_pt.dat
```

O language pack contém apenas os assets de texto/dialogo (assets 57, 94, 95, 96). Os gráficos e sprites vêm do ROM base US.

---

## Compilação

### Linux / macOS

```sh
python3 -m pip install -r requirements.txt
# macOS: brew install sdl2
# Ubuntu: sudo apt install libsdl2-dev
make
```

### Windows (TCC)

1. Extrair o projeto
2. Colocar `zelda3.sfc` na raiz
3. Rodar `extract_assets.bat`
4. Extrair TCC e SDL2 para `third_party/`
5. Rodar `run_with_tcc.bat`

### Windows (Visual Studio)

1. Seguir passos 1-4 do TCC
2. Abrir `Zelda3.sln`
3. Build > Build Zelda3

---

## Licença

MIT — veja `LICENSE.txt`
