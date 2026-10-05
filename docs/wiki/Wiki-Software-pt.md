# S-Log MetaRaw — Guia do usuário

[English](Wiki-Software) · [Italiano](Wiki-Software-it) · [Español](Wiki-Software-es) · **Português** · [简体中文](Wiki-Software-zh) · [Home](Home-pt)

> Versão **2.3.0** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Testado apenas no DaVinci Resolve Studio 21.1 no macOS.

O S-Log MetaRaw lê os dados de gravação que as câmeras Sony escrevem em cada arquivo (Kelvin, tint, EI, objetiva, abertura, obturador, perfil de cor) e os usa dentro do DaVinci Resolve. O Resolve faz isso só com os MXF da FX6/FX9; nos MP4 da FX30, FX3, série a7 e a6000 ele ignora esses dados.

**Conteúdo**

1. [O que é — e o que não é](#1-o-que-é--e-o-que-não-é)
2. [Instalação](#2-instalação)
3. [Em resumo](#3-em-resumo)
4. [O script](#4-o-script)
5. [O nó S-Log MetaRaw](#5-o-nó-s-log-metaraw)
6. [Falsa cor](#6-falsa-cor)
7. [Tons e Zonas](#7-tons-e-zonas)
8. [O nó S-Log MetaRaw Detail](#8-o-nó-s-log-metaraw-detail)
9. [Pipelines: onde cada nó vai](#9-pipelines-onde-cada-nó-vai)
10. [Receitas](#10-receitas)
11. [Casos especiais](#11-casos-especiais)
12. [Atualizações e privacidade](#12-atualizações-e-privacidade)
13. [Desinstalação](#13-desinstalação)
14. [Solução de problemas / FAQ](#14-solução-de-problemas--faq)
15. [Linux e Windows (experimental)](#15-linux-e-windows-experimental)
16. [Limites conhecidos](#16-limites-conhecidos)

---

## 1. O que é — e o que não é

Três ferramentas, uma única instalação:

| Ferramenta | Onde, no Resolve | O que faz |
|---|---|---|
| **Script** | Workspace › Scripts › S-Log MetaRaw | Lê os metadados de cada clipe e grava-os no Media Pool. Corrige o Data Level de cada clipe. |
| Nó **S-Log MetaRaw** | Color › OpenFX — **primeiro nó** | Revela um clipe a partir dos valores de gravação: balanço de branco, exposição, espaço de cor, tons por zonas, falsa cor. |
| Nó **S-Log MetaRaw Detail** | Color › OpenFX — **logo depois** | O nó criativo: recuperação local de altas luzes/sombras, Texture, Clarity, Dehaze. |

Os arquivos originais **nunca são modificados**: sem transcodificação, sem rewrap.

**Não é raw.** Um MP4 log já está demosaicado e comprimido (8 ou 10 bits, muitas vezes 4:2:0, com a redução de ruído da câmera já aplicada). O nó aplica a ciência de cor com rigor — exposição e balanço de branco em luz linear, a partir dos valores registrados, com as curvas e os gamuts publicados pela Sony — de modo que a imagem *se comporta* de um jeito que lembra o raw: o balanço de branco desloca-se limpo, a exposição move-se como um stop de luz, as altas luzes arredondam em vez de quebrar. Force além dos limites da câmera e a informação que falta aparece: banding nos céus, ruído nas sombras levantadas, altas luzes estouradas continuam estouradas. Exponha bem na gravação.

---

## 2. Instalação

1. Baixe `SLogMetaRaw-2.3.0.dmg` em [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) e abra-o.

   ![The DMG window](images/01-dmg.png)

2. Dê um duplo clique em **Installa S-Log MetaRaw.pkg**. O pacote não está assinado com um certificado da Apple: na primeira vez, clique com o botão direito › **Abrir**. Ele pede a senha do Mac porque o plugin vai para uma pasta do sistema.

   ![Right-click › Open on the unsigned package](images/02-gatekeeper.png)

3. **Reinicie o DaVinci Resolve.**

O instalador também apaga o cache de plugins do Resolve (`OFXPluginCacheV2.xml`); o Resolve o reconstrói na próxima inicialização. Sem isso, o Resolve continuaria mostrando o painel antigo e não veria o nó Detail.

Cada instalação começa limpa: o plugin e a biblioteca anteriores são substituídos por inteiro, e as instalações de desenvolvimento são removidas.

**O que vai para onde**

| Item | Caminho |
|---|---|
| Os dois nós (um único bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Biblioteca Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Script do menu | `…/DaVinci Resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py` |
| Cache por clipe (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requisitos:** macOS 12 ou posterior, Apple silicon ou Intel, DaVinci Resolve 21 ou 20 (no Resolve 20: instale o Python 3 em python.org, ele não traz nenhum próprio).
**Clipes:** Sony XAVC `.MP4` ou `.MXF` — o script lê todos. Os nós revelam **S-Log3** (S-Gamut3.Cine ou S-Gamut3), **S-Log2** e **S-Log** (S-Gamut). Com outros perfis (Cine, HLG, S-Cinetone) eles ficam neutros e avisam.

---

## 3. Em resumo

1. Importe o material. Abra **Workspace › Scripts › S-Log MetaRaw**, clique em **1 · Ler metadados** e depois em **2 · Gravar no Resolve**.
2. Na página Color, adicione o **S-Log MetaRaw** como **primeiro nó**. Ele parte do EI, dos Kelvin e do tint do clipe: nesses valores não muda nada.
3. Corrija o balanço de branco e a exposição no nó, usando as vistas de falsa cor.
4. Molde os tons com **Toni**. Para recuperação local, Texture, Clarity ou Dehaze, acrescente o **S-Log MetaRaw Detail** como próximo nó.
5. Depois o resto da correção e, por último, seu CST / LUT / DRT de saída.

```
S-Log MetaRaw  →  S-Log MetaRaw Detail  →  resto da correção  →  CST / LUT / DRT de saída
```

![Recommended node tree](images/08-node-tree.png)

> Copie o nó para outro clipe e ele se reinicia com os dados desse clipe.

---

## 4. O script

**Workspace › Scripts › S-Log MetaRaw**

![Workspace › Scripts menu](images/03-scripts-menu.png)

A janela tem uma linha de botões, uma de opções e a lista de clipes. Ela segue o idioma do Resolve (inglês, italiano, espanhol, português, chinês simplificado). O script lê os arquivos; nunca os modifica.

![Script window after Read metadata](images/04-script-window.png)

### Botões

| Controle | O que faz |
|---|---|
| **Menu de clipes** | *Todo o Media Pool* ou *Clipes selecionados no Media Pool*. |
| **1 · Ler metadados** | Uma linha por clipe: câmera, objetiva, abertura, obturador, EI, WB, espaço de cor, data level. A coluna **Estado** diz `lido`, o que mudou durante a tomada (abertura, foco…) ou por que um clipe foi pulado. Clique numa linha para ver tudo o que foi lido, agrupado como no Catalyst Browse. |
| **2 · Gravar no Resolve** | Preenche os campos do Media Pool (painel Metadata, colunas, palavras-chave para smart bins, Camera Notes, data burn-in) e corrige valores que o Resolve lê errado dos MXF, por exemplo *Camera Aperture* `F53343` na FX6. |
| **Exportar CSV** | Exporta os valores para os quais o Resolve não tem campo (EI, tint, modo WB, distância de foco, gama de captura…) no formato CSV de metadados do Resolve. Importe com **File › Import › Metadata**, com *create custom fields* ativado. |
| **Versão** (canto inferior direito) | Clique para consultar o GitHub; fica verde quando existe uma release mais nova. |

![Clip detail, Catalyst-style](images/05-script-clip-detail.png)

### Opções

| Opção | Padrão | Efeito |
|---|---|---|
| **Tag por câmera** | desligada | Acrescenta câmera, gama e primárias às palavras-chave. |
| **Sobrescrever metadados** | ligada | Substitui os valores que o Resolve já gravou. Desligada: preenche só os campos vazios. |
| **Corrigir o Data Level** | ligada | Define o *Data Level* de cada clipe como **Full** (log) ou **Video** (709, Cine, HLG). Essa é a correção de verdade: vale para o projeto inteiro, scopes e exportações incluídos. |
| **Definir também Input Color Space** | desligada | Para projetos com gerenciamento de cor. ⚠️ Um script não consegue devolvê-lo a *Project* — só você, à mão. |

Depois de **2 · Gravar no Resolve**, os dados aparecem no Media Pool:

![Media Pool Metadata panel filled](images/06-media-pool-metadata.png)

**Velocidade.** Nada é decodificado: no máximo 24 amostras da faixa de metadados por clipe, dentro de um segundo. Um arquivo de vários gigabytes custa cerca de 100 KB de leitura. Um disco que para de responder é pulado uma vez, com uma mensagem, em vez de travar a lista. Rodar o script logo depois da importação significa que os nós encontram os dados já prontos.

---

## 5. O nó S-Log MetaRaw

**Color › OpenFX › S-Log MetaRaw** — primeiro nó, antes de qualquer CST ou LUT.

![OpenFX library with both nodes](images/07-openfx-library.png)

O nó é **pontual**: cada pixel depende só de si mesmo. Nunca cria halos, e o **Generate LUT** pode exportá-lo (recomendados 65 pontos). As configurações são salvas por clipe.

> Os painéis dos nós estão em italiano. Os rótulos abaixo vêm com tradução.

![Main node panel — top](images/09-node-panel-top.png)

| Controle | O que faz |
|---|---|
| **Versão** (no topo) | Mostra `v2.3.0`. Uma vez por dia pergunta ao GitHub pela última release; se houver uma, mostra **🟢 v2.3.0 → 2.x.y** e um clique abre o download do instalador do seu sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Nunca instala nada sozinho. |
| **Camera** · **Rileggi metadata** (reler) | A câmera que foi lida. *Rileggi* relê o clipe, devolve cada controle aos valores da câmera e grava os metadados do clipe no Media Pool. Responde em cerca de 2 s. |
| **Decode Using** | *Clip* permite mudar os controles; *Camera metadata* os trava nos valores de gravação (o nó fica transparente). |
| **White Balance** | As shot, ou presets (Daylight, Cloudy, Shade, Tungsten, Fluorescent, Flash). Mover um controle desliza para *Custom*. |
| **Color Temp** · **Tint** | Adaptação cromática Bradford em luz linear, a partir do branco que a câmera registrou. Temp mais alto esquenta; Tint positivo vai para o magenta. |
| **Exposure** | Em EI: dobrar o EI = exatamente +1 stop, em luz linear, antes de qualquer curva. |
| **False color** | Vistas de temperatura, tint e exposição — veja [§6](#6-falsa-cor). |
| **Color Space** · **Gamma** | Saída, como um Color Space Transform. *Timeline* não converte — deixe nessa opção em um projeto com gerenciamento de cor. Para corrigir em DWG: *DaVinci WG · DaVinci Intermediate*. |
| **Toni** (Tons) | Contrast, Highlights, Shadows, Whites, Bianco, Blacks, Vibrance, Saturation — veja [§7](#7-tons-e-zonas). |
| **Zone** (Zonas, fechado) | Zonas Black, Shadow, Light, Specular; Contrast Pivot; Soft Clip. |
| **Avanzate** (Avançado) | Entrada do nó, correção do data level, estado, *Sblocca controlli senza metadata* (desbloquear sem metadados). |
| **Dati di ripresa** (Dados de gravação) | Só leitura: objetiva, focal, abertura, foco, obturador, EI, WB, fps, ND, LUT da câmera. |

![Avanzate and Dati di ripresa](images/14-avanzate-dati.png)

**Avanzate em detalhe**

- **Ingresso nodo** (entrada do nó) — *Automatico* pergunta ao Resolve.
- **Data level in ingresso** — corrige a escala de code values quando o Resolve decodifica um clipe na errada. O *Corrigir o Data Level* do script corrige isso para o projeto inteiro.
- **Stato** (estado) — o que o nó detectou; também avisa quando uma vista de falsa cor está ativa.
- **Sblocca controlli senza metadata** — veja [§11](#11-casos-especiais).

> **A saída Rec.709** no nó é um CST *sem* tone mapping (as altas luzes além de Bianco clipam) e exclui o nó Detail. Prefira um CST de saída no final da árvore.

---

## 6. Falsa cor

Uma vista por controle, posicionada acima do controle deslizante a que serve. **A vista substitui a imagem — desligue-a antes de renderizar.** (Um plugin OpenFX não consegue desenhar uma sobreposição no visor do Resolve.)

Primeiro, coloque *Decode Using* em **Clip**: em *Camera metadata* os controles ficam travados.

### Exposição

Faixas em stops ao redor do cinza 18%, no estilo ARRI. Tudo o resto fica cinza.

![Exposure bands](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/falsecolor_bands.png)

| Cor | Significado |
|---|---|
| Verde | Cinza médio (18%) |
| Rosa | Um stop acima — pele |
| Amarelo | Perto do clip |
| Vermelho | Clipado |
| Azul / violeta | Sombra profunda / preto |

Ative a vista, aponte para o cinza médio ou para a pele, e mova **Exposure** até a área certa ficar verde (cinza) ou rosa (pele).

![Exposure false colour in the viewer](images/10-falsecolor-exposure.png)

### Temperatura e Tint

Funcionam como no CineMatch. A imagem fica cinza; as dominantes aparecem em cor, e as dominantes quase neutras são amplificadas até 8× para ficarem visíveis.

| Vista | Você vê | Faça |
|---|---|---|
| Temperatura | Azul (dominante fria) | Suba Color Temp |
| Temperatura | Laranja (dominante quente) | Desça Color Temp |
| Tint | Verde | Suba Tint |
| Tint | Magenta | Desça Tint |

Escolha uma superfície que deveria ser neutra e mova o controle até ela ficar cinza. Alterne Temp e Tint algumas vezes; converge rápido. Cada vista reage só ao seu próprio controle.

![Temperature false colour](images/11-falsecolor-temp.png)

Aprofundamento (em italiano): [docs/FALSE_COLOR.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/FALSE_COLOR.md)

---

## 7. Tons e Zonas

### Toni (Tons)

![Toni group](images/12-toni.png)

Todos os controles vão de −100 a +100, na ordem do Camera Raw. Highlights é um ombro de película; os demais são **exposições sobre uma faixa de tons, em stops a partir do cinza 18%**: dentro da faixa a imagem se move como numa exposição, então a textura se mantém. Nenhuma combinação de controles pode solarizar.

| Controle | Comportamento |
|---|---|
| **Contrast** | Gira em torno do Pivot. +100 dobra a inclinação no pivô, −100 reduz à metade, com as pontas limitadas. |
| **Highlights** | Negativo: ombro de película — a −100 o valor mais alto registrado (cerca de +6 stops acima do cinza em S-Log3) chega exatamente em **Bianco**, sem véu cinza, sem clip. O cinza e o que está abaixo não se movem; a pele a +1 stop se desloca no máximo 0,05 stop. O matiz se mantém constante. Positivo: mais força. |
| **Bianco** (branco, em stops) | Onde esse máximo chega, e o teto do Soft Clip. **2,5** = branco Rec.709 por um CST sem tone mapping. **Com um DRT depois (ACES, AgX, DaVinci) suba para 4–5**, senão as altas luzes são comprimidas duas vezes. |
| **Shadows** | Tons abaixo de −1 stop; 100 = 2 stops. Também levanta o preto — contenha com Blacks. |
| **Whites** | De +3,5 stops até o clip; 100 = 1 stop. |
| **Blacks** | Véu linear: move o preto (−3 / +1 stop) sem mover o cinza. |
| **Vibrance** | Em torno da luminância; protege os tons de pele. |
| **Saturation** | Em torno da luminância; igual em qualquer espaço de cor. |

**Azzera toni** reinicia o grupo.

![Highlights 0 vs −100](images/15-highlights-before-after.png)

![Tone curve](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/tone_curve.png)

> **O custo honesto de um nó pontual:** o que ele comprime, comprime também a textura ali dentro. A recuperação que preserva a textura mora no nó Detail.

### Zone (Zonas)

![Zone group](images/13-zone.png)

- Quatro zonas, como na paleta HDR do Resolve: **Black, Shadow, Light, Specular** — cada uma com **Exp** (stops), **Sat**, **Range** (borda, em stops) e **Falloff** (largura da transição).
- **Contrast Pivot** — o tom em torno do qual Contrast gira.
- **Soft Clip** / **Soft Clip Color** — dobra as altas luzes sob um teto (em Bianco) que nunca alcançam. *Color* decide se as altas luzes dobradas vão para o branco (película) ou mantêm a cor. Contenção, não recuperação.
- **Zone false colour** — colore cada pixel com a zona que o move.
- **Azzera zone** reinicia o grupo.

As bordas das zonas são levadas através do Contrast, então permanecem em stops de cena qualquer que seja o Contrast usado.

### Generate LUT

O nó é pontual, então o Generate LUT o inclui. Use **65 pontos**: com os controles em ±100 o erro fica dentro de cerca de 3,5 code values de S-Log3. Com 33 pontos, o toe linear do S-Log3 (Shadows +100) chega a cerca de 10 CV. Algumas Zone extremas (Black ou Shadow +3) superam isso mesmo com 65 — mantenha o nó ativo para essas correções.

Aprofundamento (em italiano): [docs/TONE_MAPPING.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/TONE_MAPPING.md)

---

## 8. O nó S-Log MetaRaw Detail

**Color › OpenFX › S-Log MetaRaw Detail** — logo depois do S-Log MetaRaw, antes de qualquer CST / LUT / DRT.

Trabalha **por áreas**, sobre uma base que respeita as bordas: move grandes áreas de luz sem achatar o detalhe fino — a parte de Highlights/Shadows do Lightroom que um nó pontual não consegue fazer.

![Detail node panel](images/16-detail-panel.png)

| Grupo | Controles |
|---|---|
| **Gamma dinamica** (faixa dinâmica) | Local Contrast, Local Highlights, Local Shadows; vistas de verificação *gain* e *base*. |
| **Presenza** (presença) | **Texture** (detalhe fino; não sobe o grão abaixo do limiar de ruído), **Clarity** (contraste local de média escala), **Dehaze**. |
| **Zone locali** (zonas locais) | As zonas do nó principal, aplicadas a áreas em vez de pixels. |
| **Avanzate** (avançado) | Preservação do detalhe, raio, limiar de bordas (*Soglia bordi*), limiar de ruído, centro do Clarity, **Bianco** do Local Highlights, entrada do nó. |
| **Velo** (véu) | Nível e cor do véu que o Dehaze remove. Você define; nunca são estimados quadro a quadro → sem flicker. |

**Azzera dettaglio** reinicia o nó.

**Local Highlights vs. Highlights.** Local Highlights comprime as grandes áreas claras e mantém — até reforça — a textura fina: o céu escurece e as nuvens mantêm o detalhe. O Highlights do nó principal suaviza a textura das altas luzes, como a película. Local Highlights tem seu próprio **Bianco** em Avanzate e não sabe o EI escolhido no nó principal: se você mudar muito o EI, ajuste-o.

![Local Highlights on a sky, before/after](images/17-detail-before-after.png)

**Regras**

- Decodifica para luz linear o que recebe e grava de volta na **mesma codificação**. Aceita só codificações scene-log: S-Log3, S-Log2, DaVinci WG/Intermediate, ACEScct. **Não** Rec.709, Gamma 2.4 ou sRGB.
- É **espacial**: o Generate LUT o deixa de fora, junto com o resto do seu nó. Mantenha-o num nó próprio.
- Os raios seguem a altura do quadro: o mesmo visual em resolução total, proxy e visor. Sem estatísticas por quadro → sem flicker.
- Metal: cerca de 6–18 ms por quadro UHD em Apple silicon. A alternativa por CPU é muito mais lenta.

**Limites medidos**

- Local Highlights −100: halo no lado escuro de uma borda abaixo de 3% do degrau.
- Local Shadows ou zonas locais a ±100: cerca de 12% numa borda nítida de 1 stop, 4–6% em bordas de 2–3 stops. Se vir isso, baixe *Soglia bordi*.
- Texture perto de bordas fortes pode aumentar o grão de 1,25 a 1,7×.
- O Dehaze precisa de um véu real para remover.

Aprofundamento (em italiano): [docs/DETAIL.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DETAIL.md)

---

## 9. Pipelines: onde cada nó vai

### A — Corrigir em DaVinci Wide Gamut (recomendado)

```
S-Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → Detail             Ingresso nodo: DaVinci WG/Intermediate
  → sua correção (DWG)
  → CST de saída        DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, tone mapping do DaVinci
```

Com tone mapping depois, coloque **Bianco entre 4 e 5 nos dois nós**. Numa timeline sem gerenciamento de cor, você precisa declarar a entrada do nó Detail.

### B — Tudo em log

```
S-Log MetaRaw          Color Space/Gamma: Timeline
  → Detail             Ingresso nodo: Automatico (usa o S-Log3 do clipe)
  → sua correção
  → CST / LUT          S-Gamut3.Cine/S-Log3 → sua saída
```

### C — Projeto com gerenciamento de cor (DaVinci YRGB Color Managed / ACES)

Deixe Color Space/Gamma em **Timeline** no nó principal. Ative o *Definir também Input Color Space* do script só se quiser que ele defina a entrada de cada clipe — lembre-se de que você precisa devolvê-la a *Project* à mão.

---

## 10. Receitas

| Problema | Onde | Ajuste |
|---|---|---|
| Céu ou janelas estourando | Main › Toni | Highlights −50…−100 (com um DRT depois: Bianco 4–5) |
| …e manter a textura das nuvens | Detail › Gamma dinamica | Local Highlights |
| Rosto em contraluz na sombra | Main › Toni | Shadows +30…+60, Blacks −20…−40 |
| Visual mais de "película" | Main › Toni | Contrast +20…+30, Highlights −60 |
| Sombras ruidosas e saturadas | Main › Zone | Shadow Sat −30…−50 |
| Reflexos especulares | Main › Zone | Specular Exp −1…−2 |
| Paisagem chapada e enevoada | Detail › Presenza + Velo | Dehaze; defina nível/cor do véu à mão |

---

## 11. Casos especiais

**ProRes de um gravador externo / clipe ilegível.** O nó fica neutro. Marque **Avanzate › Sblocca controlli senza metadata** e digite o EI, os Kelvin e o tint de gravação: os controles se ativam e o nó começa neutro.

**Perfis não log** (Cine, HLG, S-Cinetone): os nós ficam neutros e avisam isso em *Stato*.

**Data Level errado.** Se os pretos parecem levantados ou esmagados direto da câmera, o Resolve está decodificando na escala de code values errada. Rode o script com *Corrigir o Data Level* ativado (correção para o projeto inteiro), ou use *Avanzate › Data level in ingresso* num único clipe. Aprofundamento (em italiano): [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Câmeras sem Kelvin** (por exemplo, a a6300): o Kelvin é estimado a partir do preset de luz e sinalizado.

**Desempenho.** Abrir um projeto não lê nenhum arquivo. Abrir o painel espera no máximo meio segundo; um disco lento termina em segundo plano, com limite de 15 s.

---

## 12. Atualizações e privacidade

- Os **nós** perguntam ao GitHub pela última release no máximo uma vez por dia, em segundo plano. A requisição leva só a versão (`User-Agent: SLogMetaRaw/2.3.0`).
- O **script** só consulta quando você clica na versão dele.
- Um clique apenas abre um link de download das releases do GitHub deste projeto. Nada é instalado sem você.

![Update badge](images/18-update-badge.png)

**Para desativar a verificação:** crie o arquivo vazio

```bash
touch ~/Library/Application\ Support/SLogMetaRaw/no_update_check
```

**Atualizando a partir da 1.x:** os tons da 1.x (Highlights, Shadows, Contrast, Saturation, Color Boost, Color Recovery) não podem ser convertidos e voltam a zero. Para manter o visual de um clipe já finalizado, **antes de atualizar** rode o Generate LUT nesse nó. Depois da atualização, *Stato* mostra o que havia (por exemplo, "Toni 1.1 azzerati (H −40, C +15)"). Exposure, data level, Color Space e Gamma permanecem inalterados.

---

## 13. Desinstalação

Dê um duplo clique em **Disinstalla S-Log MetaRaw.command** no DMG (na primeira vez: botão direito › Abrir). Ele:

1. lista tudo o que vai remover (todas as versões 1.x e 2.x, o nome antigo *SonyMeta*, instalações de desenvolvimento, cache, configurações, logs);
2. pede para você fechar o Resolve;
3. pergunta se deve apagar também os CSVs exportados;
4. verifica que nada ficou para trás.

Execute com `--dry-run` para ver o que seria removido sem tocar em nada. Os metadados já gravados nos projetos do Resolve permanecem — fazem parte dos projetos.

![Uninstaller in Terminal](images/20-uninstaller.png)

---

## 14. Solução de problemas / FAQ

**O nó Detail está faltando / o painel parece antigo.**
O Resolve está usando seu cache de plugins antigo. Saia do Resolve, apague o `OFXPluginCacheV2.xml` (o instalador normalmente faz isso), reinicie.

**O nó não faz nada.**
Isso é esperado nos valores de gravação. Verifique se *Decode Using* está em **Clip**, e leia *Avanzate › Stato*: ele diz se o perfil não é log ou se faltam metadados.

**Minha renderização saiu com falsa cor.**
Uma vista ficou ativada. Desligue todas as vistas de falsa cor antes de renderizar.

**As altas luzes parecem comprimidas duas vezes / sem vida.**
Você tem um DRT (ACES, AgX, tone mapping do DaVinci) depois do nó: suba **Bianco** para 4–5 (no nó Detail também).

**Halos ao redor das bordas com o nó Detail.**
Baixe *Avanzate › Soglia bordi*, ou reduza Local Shadows / zonas locais.

**O script pulou um clipe.**
Leia a coluna *Status*: ela diz por quê (arquivo não suportado, disco não responde, etc.).

**"Camera Aperture F53343" na FX6.**
Uma leitura errada do MXF pelo Resolve; **2 · Gravar no Resolve** corrige.

**O macOS não abre o instalador.**
Ele não é assinado: botão direito › Abrir (ou System Settings › Privacy & Security › Open Anyway).

---

## 15. Linux e Windows (experimental)

O plugin também compila para Linux e Windows com CMake. Esses ports renderizam só na **CPU** (ainda sem Metal, CUDA
ou OpenCL) e não foram testados dentro do próprio DaVinci Resolve: a build compatível continua sendo a do macOS.

**Linux** (baixe em [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases); escolha o que se encaixa na sua distribuição):

| Arquivo | Para |
|---|---|
| `slogmetaraw_<version>_amd64.deb` | Ubuntu, Debian, Mint: `sudo apt install ./slogmetaraw_*.deb` |
| `slogmetaraw-<version>-1.x86_64.rpm` | Rocky, Alma, RHEL, CentOS, Fedora: `sudo dnf install ./slogmetaraw-*.rpm` |
| `SLogMetaRaw-<version>-linux-x86_64.run` | qualquer distribuição: `sh SLogMetaRaw-*.run` (`--user` sem sudo, `--uninstall`) |
| `SLogMetaRaw-<version>-linux-x86_64.tar.gz` | o mesmo, descompactado: execute `install.sh` dentro |

O plugin vai para `/usr/OFX/Plugins`; a biblioteca Python para `/usr/lib/slogmetaraw` (.deb/.rpm) ou
`~/.local/share/SLogMetaRaw` (.run, .tar.gz). Reinicie o Resolve. Basta o Python 3.6+. O binário é compilado contra a
glibc 2.28, então carrega no Rocky/Alma/RHEL 8+, Ubuntu 20.04+ e Debian 10+.

**Windows:** compile você mesmo com `powershell -File packaging\windows\build_package.ps1` (cmake, Visual Studio
Build Tools, python; o Inno Setup é opcional para o `.exe`). O estado fica em `%APPDATA%\SLogMetaRaw`.

Uma tag `v*` enviada ao repositório faz o CI anexar os pacotes Linux (e, best-effort, o pacote Windows) a uma
release em **rascunho** no GitHub automaticamente — veja [Limites conhecidos](#16-limites-conhecidos) para o que
ainda falta (aceleração por GPU, um pipeline de imagem testado equivalente ao do macOS).

---

## 16. Limites conhecidos

- O painel Camera Raw do Resolve e a estabilização por giroscópio não podem ser desbloqueados para MP4: vivem dentro dos decodificadores do Resolve. O S-Log MetaRaw reconstrói os controles de cor; não abre uma porta dentro do Resolve.
- O S-Log2 segue o documento da Sony; a curva S-Log2 do Resolve difere em cerca de 0,15 stop.
- Ainda por verificar em mais arquivos: XAVC HS (HEVC), HLG, S-Cinetone, zooms motorizados.
- Testado apenas no Resolve Studio 21.1 no macOS. As builds Linux e Windows são só CPU e não testadas dentro do Resolve (veja [§15](#15-linux-e-windows-experimental)).
- Projeto independente e amador, fornecido como está, sem garantia e sem responsabilidade pelo uso profissional. Não afiliado nem endossado pela Sony ou pela Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0 ou posterior](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Notas de versão](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)
