# S-Log MetaRaw — Guia do usuário

[English](Wiki-Software) · [Italiano](Wiki-Software-it) · [Español](Wiki-Software-es) · **Português** · [简体中文](Wiki-Software-zh)

> Versão **2.3.0** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Testado apenas no DaVinci Resolve Studio 21.1 em macOS.

O S-Log MetaRaw lê os dados de gravação que as câmeras Sony escrevem em cada arquivo (Kelvin, tint, EI, objetiva, abertura, obturador, perfil de cor) e os usa dentro do DaVinci Resolve. O Resolve faz isso só para os arquivos MXF da FX6/FX9; nos MP4 da FX30, FX3, série a7 e a6000 ignora esses dados.

**Índice**

1. [O que é — e o que não é](#1-o-que-é--e-o-que-não-é)
2. [Instalação](#2-instalação)
3. [Começo rápido](#3-começo-rápido)
4. [O script](#4-o-script)
5. [O nó S-Log MetaRaw](#5-o-nó-s-log-metaraw)
6. [Falsa cor](#6-falsa-cor)
7. [Tons e Zonas](#7-tons-e-zonas)
8. [O nó S-Log MetaRaw Detail](#8-o-nó-s-log-metaraw-detail)
9. [Pipelines: onde vai cada nó](#9-pipelines-onde-vai-cada-nó)
10. [Receitas](#10-receitas)
11. [Casos especiais](#11-casos-especiais)
12. [Atualizações e privacidade](#12-atualizações-e-privacidade)
13. [Desinstalação](#13-desinstalação)
14. [Solução de problemas / FAQ](#14-solução-de-problemas--faq)
15. [Limites conhecidos](#15-limites-conhecidos)

---

## 1. O que é — e o que não é

Três ferramentas, uma única instalação:

| Ferramenta | Onde no Resolve | O que faz |
|---|---|---|
| **Script** | Workspace › Scripts › S-Log MetaRaw | Lê os metadados de todos os clipes e grava-os no Media Pool. Corrige o Data Level de cada clipe. |
| Nó **S-Log MetaRaw** | Color › OpenFX — **primeiro nó** | Revela um clipe a partir dos valores de gravação: balanço de branco, exposição, espaço de cor, tons por zonas, falsa cor. |
| Nó **S-Log MetaRaw Detail** | Color › OpenFX — **logo depois** | O nó criativo: recuperação local de altas luzes/sombras, Texture, Clarity, Dehaze. |

Os arquivos originais **nunca são modificados**: sem transcodificação, sem rewrapping.

**Não é raw.** Um MP4 log já está demosaicado e comprimido (8 ou 10 bits, muitas vezes 4:2:0, com redução de ruído feita na câmera). O nó aplica a ciência de cor com rigor — exposição e balanço de branco em luz linear, a partir dos valores de gravação, com as curvas e os gamuts publicados pela Sony — de modo que a imagem *se comporta* de um jeito que lembra o raw: o balanço de branco desloca-se limpo, a exposição move-se como um stop de luz, as altas luzes arredondam em vez de quebrar. Force além dos limites da câmera e a falta de informação aparece: banding nos céus, ruído nas sombras levantadas, altas luzes estouradas continuam estouradas. Exponha bem na gravação.

---

## 2. Instalação

1. Baixe o `SLogMetaRaw-2.3.0.dmg` em [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) e abra-o.

   ![A janela do DMG](images/01-dmg.png)

2. Dê um duplo clique em **Installa S-Log MetaRaw.pkg**. O pacote não está assinado com um certificado da Apple: na primeira vez, **clique com o botão direito › Abrir**. Pede a senha do seu Mac porque o plugin vai para uma pasta do sistema.

   ![Clique com o botão direito › Abrir no pacote não assinado](images/02-gatekeeper.png)

3. **Reinicie o DaVinci Resolve.**

O instalador também apaga o cache de plugins do Resolve (`OFXPluginCacheV2.xml`); o Resolve o reconstrói na próxima inicialização. Sem isso, o Resolve mostraria o painel antigo e não veria o nó Detail.

Cada instalação começa limpa: o plugin e a biblioteca anteriores são substituídos por inteiro, e as instalações de desenvolvimento são removidas.

**O que vai onde**

| Item | Caminho |
|---|---|
| Os dois nós (um único bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Biblioteca Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Script do menu | `…/DaVinci Resolve/Fusion/Scripts/Utility/S-Log MetaRaw.py` |
| Cache por clipe (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requisitos:** macOS 12 ou posterior, Apple silicon ou Intel, DaVinci Resolve 21 ou 20 (Resolve 20: instale o Python 3 em python.org, pois não traz nenhum próprio).
**Clipes:** Sony XAVC em `.MP4` ou `.MXF` — o script lê todos. Os nós revelam **S-Log3** (S-Gamut3.Cine ou S-Gamut3), **S-Log2** e **S-Log** (S-Gamut). Com outros perfis (Cine, HLG, S-Cinetone) ficam neutros e avisam.

---

## 3. Começo rápido

1. Importe o material. Abra **Workspace › Scripts › S-Log MetaRaw**, clique em **1 · Ler metadados** e depois em **2 · Gravar no Resolve**.
2. Na página Color, acrescente o **S-Log MetaRaw** como **primeiro nó**. Ele pega o EI, os Kelvin e o tint do clipe: nesses valores não muda nada.
3. Corrija o balanço de branco e a exposição no nó, usando as vistas de falsa cor.
4. Molde os tons com **Toni**. Para recuperação local, Texture, Clarity ou Dehaze, acrescente o **S-Log MetaRaw Detail** como nó seguinte.
5. Depois, o resto da correção e, por último, o seu CST / LUT / DRT de saída.

```
S-Log MetaRaw  →  S-Log MetaRaw Detail  →  resto da correção  →  CST / LUT / DRT de saída
```

![Árvore de nós recomendada](images/08-node-tree.png)

> Copie o nó para outro clipe e ele se reinicia com os dados desse clipe.

---

## 4. O script

**Workspace › Scripts › S-Log MetaRaw**

![Menu Workspace › Scripts](images/03-scripts-menu.png)

A janela tem uma linha de botões, uma linha de opções e a lista de clipes. Segue o idioma do Resolve (inglês, italiano, espanhol, português, chinês simplificado). O script lê os arquivos; nunca os modifica.

![Janela do script depois de Ler metadados](images/04-script-window.png)

### Botões

| Controle | O que faz |
|---|---|
| **Menu de clipes** | *Todo o Media Pool* ou *Clipes selecionados no Media Pool*. |
| **1 · Ler metadados** | Uma linha por clipe: câmera, objetiva, abertura, obturador, EI, WB, espaço de cor, data level. A coluna **Estado** diz `lido`, o que mudou durante a tomada (abertura, foco…), ou por que um clipe foi pulado. Clique numa linha para ver tudo o que foi lido, agrupado como no Catalyst Browse. |
| **2 · Gravar no Resolve** | Preenche os campos do Media Pool (painel Metadata, colunas, palavras-chave para smart bins, Camera Notes, data burn-in) e corrige valores que o Resolve lê errado dos MXF, por exemplo *Camera Aperture* `F53343` na FX6. |
| **Exportar CSV** | Exporta os valores para os quais o Resolve não tem campo (EI, tint, modo WB, distância de foco, gama de captura…) no formato CSV de metadados do Resolve. Importe com **File › Import › Metadata**, com *create custom fields* ativado. |
| **Versão** (canto inferior direito) | Clique para consultar o GitHub; fica verde quando existe uma release mais nova. |

![Detalhe do clipe, no estilo Catalyst](images/05-script-clip-detail.png)

### Opções

| Opção | Padrão | Efeito |
|---|---|---|
| **Tag por câmera** | desligada | Acrescenta câmera, gama e primárias às palavras-chave. |
| **Sobrescrever metadados** | ligada | Substitui os valores que o Resolve já gravou. Desligada: preenche só os campos vazios. |
| **Corrigir o Data Level** | ligada | Define o *Data Level* de cada clipe como **Full** (log) ou **Video** (709, Cine, HLG). É a correção de verdade: vale para o projeto inteiro, scopes e exportações incluídos. |
| **Definir também Input Color Space** | desligada | Para projetos com gerenciamento de cor. ⚠️ Um script não consegue voltá-lo para *Project* — só você, à mão. |

Depois de **2 · Gravar no Resolve**, os dados aparecem no Media Pool:

![Painel Metadata do Media Pool preenchido](images/06-media-pool-metadata.png)

**Velocidade.** Nada é decodificado: no máximo 24 amostras da faixa de metadados por clipe, dentro de um segundo. Um arquivo de vários GB custa cerca de 100 KB de leitura. Um disco que para de responder é pulado uma vez, com uma mensagem, em vez de travar a lista. Rodar o script logo depois da importação faz os nós encontrarem os dados já prontos.

---

## 5. O nó S-Log MetaRaw

**Color › OpenFX › S-Log MetaRaw** — primeiro nó, antes de qualquer CST ou LUT.

![Biblioteca OpenFX com os dois nós](images/07-openfx-library.png)

O nó é **pontual**: cada pixel depende só de si mesmo. Nunca cria halos, e o **Generate LUT** pode exportá-lo (recomendados 65 pontos). As configurações são salvas por clipe.

> Os painéis dos nós estão em italiano. Os rótulos abaixo vêm com tradução.

![Painel do nó principal — parte superior](images/09-node-panel-top.png)

| Controle | O que faz |
|---|---|
| **Versão** (no topo) | Mostra `v2.3.0`. Uma vez por dia pergunta ao GitHub pela última release; se houver, mostra **🟢 v2.3.0 → 2.x.y** e um clique abre o download do instalador para o seu sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Nunca instala nada sozinho. |
| **Camera** · **Rileggi metadata** (reler) | A câmera que foi lida. *Rileggi* lê o clipe de novo, devolve cada controle aos valores da câmera e grava os metadados do clipe no Media Pool. Responde em ~2 s. |
| **Decode Using** | *Clip* permite mudar os controles; *Camera metadata* trava-os nos valores de gravação (o nó fica transparente). |
| **White Balance** | As shot, ou presets (Daylight, Cloudy, Shade, Tungsten, Fluorescent, Flash). Mover um controle muda para *Custom*. |
| **Color Temp** · **Tint** | Adaptação cromática Bradford em luz linear, a partir do branco que a câmera registrou. Mais Temp esquenta; Tint positivo vai para o magenta. |
| **Exposure** | Em EI: o dobro do EI = exatamente +1 stop, em luz linear, antes de qualquer curva. |
| **False color** | Vistas de temperatura, tint e exposição — veja [§6](#6-falsa-cor). |
| **Color Space** · **Gamma** | Saída, como um Color Space Transform. *Timeline* não converte — deixe assim num projeto com gerenciamento de cor. Para corrigir em DWG: *DaVinci WG · DaVinci Intermediate*. |
| **Toni** (Tons) | Contrast, Highlights, Shadows, Whites, Bianco, Blacks, Vibrance, Saturation — veja [§7](#7-tons-e-zonas). |
| **Zone** (Zonas, fechado) | Zonas Black, Shadow, Light, Specular; Contrast Pivot; Soft Clip. |
| **Avanzate** (Avançado) | Entrada do nó, correção do data level, estado, *Sblocca controlli senza metadata* (desbloquear sem metadados). |
| **Dati di ripresa** (dados de gravação) | Só leitura: objetiva, focal, abertura, foco, obturador, EI, WB, fps, ND, LUT da câmera. |

![Avanzate e Dati di ripresa](images/14-avanzate-dati.png)

**Avanzate em detalhe**

- **Ingresso nodo** (entrada do nó) — *Automatico* pergunta ao Resolve.
- **Data level in ingresso** — corrige a escala de code values quando o Resolve decodifica um clipe na errada. O *Corrigir o Data Level* do script corrige isso para o projeto inteiro.
- **Stato** (estado) — o que o nó detectou; também avisa quando uma vista de falsa cor está ligada.
- **Sblocca controlli senza metadata** — veja [§11](#11-casos-especiais).

> **Saída Rec.709** no nó é um CST *sem* tone mapping (as altas luzes além de Bianco são cortadas) e exclui o nó Detail. Prefira um CST de saída no final da árvore.

---

## 6. Falsa cor

Uma vista por controle, colocada acima do controle deslizante a que serve. **A vista substitui a imagem — desligue-a antes de renderizar.** (Um plugin OpenFX não consegue desenhar uma sobreposição no visor do Resolve.)

Primeiro, ajuste *Decode Using* para **Clip**: em *Camera metadata* os controles ficam travados.

### Exposição

Faixas ao estilo ARRI, em stops em torno do cinza 18%. Tudo o mais fica cinza.

![Faixas de exposição](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/falsecolor_bands.png)

| Cor | Significado |
|---|---|
| Verde | Cinza médio (18%) |
| Rosa | Um stop acima — pele |
| Amarelo | Perto do clip |
| Vermelho | Clipado |
| Azul / violeta | Sombra profunda / preto |

Ligue a vista, aponte para o cinza médio ou para a pele, mova **Exposure** até a área certa ficar verde (cinza) ou rosa (pele).

![Falsa cor de exposição no visor](images/10-falsecolor-exposure.png)

### Temperatura e Tint

Funcionam como no CineMatch. A imagem fica cinza; as dominantes aparecem em cor, e as dominantes quase neutras são amplificadas até 8× para ficarem visíveis.

| Vista | Você vê | Faça |
|---|---|---|
| Temperatura | Azul (dominante fria) | Suba o Color Temp |
| Temperatura | Laranja (dominante quente) | Baixe o Color Temp |
| Tint | Verde | Suba o Tint |
| Tint | Magenta | Baixe o Tint |

Escolha uma superfície que deveria ser neutra e mova o controle até ela ficar cinza. Alterne Temp e Tint algumas vezes; converge rápido. Cada vista reage só ao seu próprio controle.

![Falsa cor de temperatura](images/11-falsecolor-temp.png)

Aprofundamento (em italiano): [docs/FALSE_COLOR.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/FALSE_COLOR.md)

---

## 7. Tons e Zonas

### Toni (Tons)

![Grupo Toni](images/12-toni.png)

Todos os controles vão de −100 a +100, na ordem do Camera Raw. Highlights é um ombro de película; os outros são **exposições sobre uma faixa de tons, em stops a partir do cinza 18%**: dentro da faixa a imagem se move como numa exposição, então a textura se mantém. Nenhuma combinação de controles pode solarizar.

| Controle | Comportamento |
|---|---|
| **Contrast** | Gira em torno do Pivot. +100 dobra a inclinação no pivot, −100 reduz-a à metade, com as pontas limitadas. |
| **Highlights** | Negativo: ombro de película — a −100 o valor mais claro registrado (~+6 stops acima do cinza em S-Log3) chega exatamente ao **Bianco**, sem véu cinza, sem clip. O cinza e o que está abaixo não se movem; a pele a +1 stop se desloca no máximo 0,05 stop. O matiz se mantém constante. Positivo: mais força. |
| **Bianco** (branco, em stops) | Onde esse máximo chega, e o teto do Soft Clip. **2,5** = branco Rec.709 através de um CST sem tone mapping. **Com um DRT depois (ACES, AgX, DaVinci) suba para 4–5**, senão as altas luzes são comprimidas duas vezes. |
| **Shadows** | Tons abaixo de −1 stop; 100 = 2 stops. Também levanta o preto — segure-o com Blacks. |
| **Whites** | De +3,5 stops até o clip; 100 = 1 stop. |
| **Blacks** | Véu linear: move o preto (−3 / +1 stop) sem mover o cinza. |
| **Vibrance** | Em torno da luminância; protege os tons de pele. |
| **Saturation** | Em torno da luminância; igual em qualquer espaço de cor. |

**Azzera toni** reinicia o grupo.

![Highlights 0 vs −100](images/15-highlights-before-after.png)

![Curva de tons](https://raw.githubusercontent.com/Mecena-SRL/SLogMetaRaw/main/docs/tone_curve.png)

> **O custo honesto de um nó pontual:** o que ele comprime, comprime também a textura dentro dele. A recuperação que preserva a textura mora no nó Detail.

### Zone (Zonas)

![Grupo Zone](images/13-zone.png)

- Quatro zonas, como na paleta HDR do Resolve: **Black, Shadow, Light, Specular** — cada uma com **Exp** (stops), **Sat**, **Range** (borda, em stops) e **Falloff** (largura da transição).
- **Contrast Pivot** — o tom em torno do qual o Contrast gira.
- **Soft Clip** / **Soft Clip Color** — dobra as altas luzes sob um teto (em Bianco) que elas nunca alcançam. *Color* decide se as altas luzes dobradas vão para o branco (película) ou mantêm a cor. Contenção, não recuperação.
- **Zone false colour** — colore cada pixel com a zona que o move.
- **Azzera zone** reinicia o grupo.

As bordas das zonas acompanham o Contrast, então permanecem em stops de cena qualquer que seja o Contrast usado.

### Generate LUT

O nó é pontual, então o Generate LUT o inclui. Use **65 pontos**: com os controles em ±100 o erro fica dentro de ~3,5 code values de S-Log3. Com 33 pontos, o toe linear do S-Log3 (Shadows +100) chega a ~10 CV. Algumas Zone extremas (Black ou Shadow +3) ultrapassam isso mesmo a 65 — mantenha o nó ativo nessas correções.

Aprofundamento (em italiano): [docs/TONE_MAPPING.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/TONE_MAPPING.md)

---

## 8. O nó S-Log MetaRaw Detail

**Color › OpenFX › S-Log MetaRaw Detail** — logo depois do S-Log MetaRaw, antes de qualquer CST / LUT / DRT.

Funciona **por áreas**, sobre uma base que respeita as bordas: move grandes áreas de luz sem achatar o detalhe fino — a parte do Highlights/Shadows do Lightroom que um nó pontual não consegue fazer.

![Painel do nó Detail](images/16-detail-panel.png)

| Grupo | Controles |
|---|---|
| **Gamma dinamica** (faixa dinâmica) | Local Contrast, Local Highlights, Local Shadows; vistas de verificação *gain* e *base*. |
| **Presenza** (presença) | **Texture** (detalhe fino; não aumenta o grão abaixo do limiar de ruído), **Clarity** (contraste local em escala média), **Dehaze**. |
| **Zone locali** (zonas locais) | As zonas do nó principal, aplicadas a áreas em vez de pixels. |
| **Avanzate** (avançado) | Preservação de detalhe, raio, limiar de bordas (*Soglia bordi*), limiar de ruído, centro do Clarity, **Bianco** do Local Highlights, entrada do nó. |
| **Velo** (véu) | Nível e cor do véu que o Dehaze remove. Você define; nunca são estimados quadro a quadro → sem flicker. |

**Azzera dettaglio** reinicia o nó.

**Local Highlights vs Highlights.** O Local Highlights comprime grandes áreas claras e mantém — até reforça — a textura fina: o céu escurece e as nuvens mantêm o detalhe. O Highlights do nó principal suaviza a textura das altas luzes, como a película. O Local Highlights tem seu próprio **Bianco** em Avanzate e não conhece o EI escolhido no nó principal: se você mudar muito o EI, ajuste-o.

![Local Highlights num céu, antes/depois](images/17-detail-before-after.png)

**Regras**

- Decodifica para luz linear o que recebe e grava de volta na **mesma codificação**. Aceita só codificações scene-log: S-Log3, S-Log2, DaVinci WG/Intermediate, ACEScct. **Não** Rec.709, Gamma 2.4 ou sRGB.
- É **espacial**: o Generate LUT o deixa de fora, junto com o resto do seu nó. Mantenha-o num nó próprio.
- Os raios seguem a altura do quadro: o mesmo visual em resolução total, proxy e visor. Sem estatísticas por quadro → sem flicker.
- Metal: ~6–18 ms por quadro UHD em Apple silicon. O fallback por CPU é bem mais lento.

**Limites medidos**

- Local Highlights −100: halo no lado escuro de uma borda < 3% do degrau.
- Local Shadows ou as zonas locais a ±100: ~12% numa borda nítida de 1 stop, 4–6% em bordas de 2–3 stops. Se você vir isso, baixe *Soglia bordi*.
- O Texture perto de bordas fortes pode aumentar o grão em 1,25–1,7×.
- O Dehaze precisa de um véu real para remover.

Aprofundamento (em italiano): [docs/DETAIL.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DETAIL.md)

---

## 9. Pipelines: onde vai cada nó

### A — Corrija em DaVinci Wide Gamut (recomendado)

```
S-Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → Detail             Ingresso nodo: DaVinci WG/Intermediate
  → sua correção (DWG)
  → CST de saída        DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, tone mapping do DaVinci
```

Com tone mapping depois, ajuste **Bianco para 4–5 nos dois nós**. Numa timeline não gerenciada, você precisa declarar a entrada do nó Detail.

### B — Tudo em log

```
S-Log MetaRaw          Color Space/Gamma: Timeline
  → Detail             Ingresso nodo: Automatico (usa o S-Log3 do clipe)
  → sua correção
  → CST / LUT          S-Gamut3.Cine/S-Log3 → sua saída
```

### C — Projeto com gerenciamento de cor (DaVinci YRGB Color Managed / ACES)

Deixe Color Space/Gamma em **Timeline** no nó principal. Ative o *Definir também Input Color Space* do script só se quiser que ele defina a entrada de cada clipe — lembre-se de que você precisa voltá-la para *Project* à mão.

---

## 10. Receitas

| Problema | Onde | Ajuste |
|---|---|---|
| Céu ou janelas estourando | Main › Toni | Highlights −50…−100 (com um DRT depois: Bianco 4–5) |
| …e manter a textura das nuvens | Detail › Gamma dinamica | Local Highlights |
| Rosto em contraluz na sombra | Main › Toni | Shadows +30…+60, Blacks −20…−40 |
| Visual mais "película" | Main › Toni | Contrast +20…+30, Highlights −60 |
| Sombras saturadas e ruidosas | Main › Zone | Shadow Sat −30…−50 |
| Reflexos especulares | Main › Zone | Specular Exp −1…−2 |
| Paisagem chapada e enevoada | Detail › Presenza + Velo | Dehaze; ajuste nível/cor do véu à mão |

---

## 11. Casos especiais

**ProRes de um gravador externo / clipe ilegível.** O nó fica neutro. Marque **Avanzate › Sblocca controlli senza metadata** e digite o EI, os Kelvin e o tint de gravação: os controles se ativam e o nó começa neutro.

**Perfis que não são log** (Cine, HLG, S-Cinetone): os nós ficam neutros e avisam em *Stato*.

**Data Level errado.** Se os pretos parecem levantados ou esmagados direto da câmera, o Resolve está decodificando na escala de code values errada. Rode o script com *Corrigir o Data Level* ativado (correção para o projeto inteiro), ou use *Avanzate › Data level in ingresso* num único clipe. Aprofundamento (em italiano): [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Câmeras sem Kelvin** (por exemplo, a a6300): os Kelvin são estimados a partir do preset de luz e sinalizados.

**Desempenho.** Abrir um projeto não lê nenhum arquivo. Abrir o painel espera no máximo meio segundo; um disco lento termina em segundo plano, com limite de 15 s.

---

## 12. Atualizações e privacidade

- Os **nós** perguntam ao GitHub pela última release no máximo uma vez por dia, em segundo plano. A requisição leva só a versão (`User-Agent: SLogMetaRaw/2.3.0`).
- O **script** só consulta quando você clica na sua versão.
- Um clique apenas abre um link de download das releases do GitHub deste projeto. Nada é instalado sem você.

![Selo de atualização](images/18-update-badge.png)

**Para desativar a verificação:** crie o arquivo vazio

```bash
touch ~/Library/Application\ Support/SLogMetaRaw/no_update_check
```

**Atualizando a partir da 1.x:** os tons da 1.x (Highlights, Shadows, Contrast, Saturation, Color Boost, Color Recovery) não podem ser convertidos e são zerados. Para manter o visual de um clipe já finalizado, **antes de atualizar** rode o Generate LUT nesse nó. Depois da atualização, *Stato* mostra o que havia (por exemplo, "Toni 1.1 azzerati (H −40, C +15)"). Exposure, data level, Color Space e Gamma não mudam.

---

## 13. Desinstalação

Dê um duplo clique em **Disinstalla S-Log MetaRaw.command**, no DMG (na primeira vez: clique com o botão direito › Abrir). Ele:

1. lista tudo o que vai remover (todas as versões 1.x e 2.x, o nome antigo *SonyMeta*, instalações de desenvolvimento, cache, configurações, logs);
2. pede para você fechar o Resolve;
3. pergunta se deve apagar também os CSV exportados;
4. verifica que não sobrou nada.

Rode com `--dry-run` para ver o que seria removido sem mexer em nada. Os metadados já gravados nos projetos do Resolve permanecem — fazem parte dos projetos.

![Desinstalador no Terminal](images/20-uninstaller.png)

---

## 14. Solução de problemas / FAQ

**O nó Detail está faltando / o painel parece antigo.**
O Resolve está usando o cache antigo de plugins. Feche o Resolve, apague `OFXPluginCacheV2.xml` (o instalador normalmente faz isso), reinicie.

**O nó não faz nada.**
Isso é esperado nos valores de gravação. Confira se *Decode Using* está em **Clip**, e leia *Avanzate › Stato*: ele diz se o perfil não é log ou se faltam metadados.

**Minha renderização saiu com falsa cor.**
Uma vista ficou ligada. Desligue todas as vistas de falsa cor antes de renderizar.

**As altas luzes parecem comprimidas duas vezes / sem vida.**
Você tem um DRT (ACES, AgX, tone mapping do DaVinci) depois do nó: suba o **Bianco** para 4–5 (no nó Detail também).

**Halos em torno de bordas com o nó Detail.**
Baixe *Avanzate › Soglia bordi*, ou reduza Local Shadows / as zonas locais.

**O script pulou um clipe.**
Leia a coluna *Estado*: ela diz por quê (arquivo não suportado, disco sem resposta, etc.).

**"Camera Aperture F53343" na FX6.**
Uma leitura errada do Resolve nos MXF; **2 · Gravar no Resolve** corrige.

**O macOS não abre o instalador.**
Ele não é assinado: clique com o botão direito › Abrir (ou System Settings › Privacy & Security › Open Anyway).

---

## 15. Limites conhecidos

- O painel Camera Raw do Resolve e a estabilização por giroscópio não podem ser desbloqueados para MP4: vivem dentro dos decodificadores do Resolve. O S-Log MetaRaw reconstrói os controles de cor, não abre uma porta dentro do Resolve.
- O S-Log2 segue o documento da Sony; a curva S-Log2 do Resolve difere em ~0,15 stop.
- Ainda por verificar em mais arquivos: XAVC HS (HEVC), HLG, S-Cinetone, zooms motorizados.
- Testado apenas no Resolve Studio 21.1 em macOS.
- Projeto independente e amador, fornecido como está, sem garantia e sem responsabilidade pelo uso profissional. Não é afiliado nem endossado pela Sony ou pela Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0+](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Notas de versão](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)
