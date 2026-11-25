# Log MetaRaw — Guia do usuário

[English](Wiki-Software) · [Italiano](Wiki-Software-it) · [Español](Wiki-Software-es) · **Português** · [简体中文](Wiki-Software-zh) · [Home](Home-pt)

> Versão **4.3.1** · macOS 12+ · DaVinci Resolve 20 / 21 · Sony XAVC `.MP4` / `.MXF`
> Testado apenas no DaVinci Resolve Studio 21.1 no macOS.

O Log MetaRaw lê os dados de gravação que as câmeras Sony escrevem em cada arquivo (Kelvin, tint, EI, objetiva, abertura, obturador, perfil de cor) e os usa dentro do DaVinci Resolve. O Resolve faz isso só com os MXF da FX6/FX9; nos MP4 da FX30, FX3, série a7 e a6000 ele ignora esses dados.

**Conteúdo**

1. [O que é — e o que não é](#1-o-que-é--e-o-que-não-é)
2. [Instalação](#2-instalação)
3. [Em resumo](#3-em-resumo)
4. [O script](#4-o-script)
5. [O nó Log MetaRaw](#5-o-nó-s-log-metaraw)
6. [Falsa cor](#6-falsa-cor)
7. [Tons e Zonas](#7-tons-e-zonas)
8. [O nó Log MetaRaw Creative](#8-o-nó-log-metaraw-creative)
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
| **Script** | Workspace › Scripts › Log MetaRaw | Lê os metadados de cada clipe e grava-os no Media Pool. Corrige o Data Level de cada clipe. |
| Nó **Log MetaRaw** | Color › OpenFX — **primeiro nó** | Revela um clipe a partir dos valores de gravação: balanço de branco, exposição, espaço de cor, tons por zonas, falsa cor. |
| Nó **5 · Log MetaRaw Creative** | Color › OpenFX | revelação criativa: 16 processos fotográficos (negativos e cópias, reversíveis, preto e branco, Technicolor, Autochrome, três chapas, daguerreótipo, colódio, cópias antigas), vídeo analógico e câmeras digitais de época; cópias e gerações de cópia; presets por dispositivo e modelo (câmeras de cinema, videocâmeras de tubo e CCD, câmeras digitais e telefones), 14 lentes de época, cópias (telecine, cinescópio, film-out) e um suporte de visualização (sala, projetores, TVs CRT e Trinitron, plasma, primeiros LCD, console, papel) |

Os arquivos originais **nunca são modificados**: sem transcodificação, sem rewrap.

**Não é raw.** Um MP4 log já está demosaicado e comprimido (8 ou 10 bits, muitas vezes 4:2:0, com a redução de ruído da câmera já aplicada). O nó aplica a ciência de cor com rigor — exposição e balanço de branco em luz linear, a partir dos valores registrados, com as curvas e os gamuts publicados pela Sony — de modo que a imagem *se comporta* de um jeito que lembra o raw: o balanço de branco desloca-se limpo, a exposição move-se como um stop de luz, as altas luzes arredondam em vez de quebrar. Force além dos limites da câmera e a informação que falta aparece: banding nos céus, ruído nas sombras levantadas, altas luzes estouradas continuam estouradas. Exponha bem na gravação.

---

## 2. Instalação

1. Baixe `SLogMetaRaw-3.0.0.dmg` em [Releases](https://github.com/Mecena-SRL/SLogMetaRaw/releases) e abra-o.

   ![The DMG window](images/01-dmg.png)

2. Dê um duplo clique em **Installa Log MetaRaw.pkg**. O pacote não está assinado com um certificado da Apple: na primeira vez, clique com o botão direito › **Abrir**. Ele pede a senha do Mac porque o plugin vai para uma pasta do sistema.

   ![Right-click › Open on the unsigned package](images/02-gatekeeper.png)

3. **Reinicie o DaVinci Resolve.**

O instalador também apaga o cache de plugins do Resolve (`OFXPluginCacheV2.xml`); o Resolve o reconstrói na próxima inicialização. Sem isso, o Resolve continuaria mostrando o painel antigo e não veria o nó Creative.

Cada instalação começa limpa: o plugin e a biblioteca anteriores são substituídos por inteiro, e as instalações de desenvolvimento são removidas.

**O que vai para onde**

| Item | Caminho |
|---|---|
| Os dois nós (um único bundle) | `/Library/OFX/Plugins/SLogMetaRaw.ofx.bundle` |
| Biblioteca Python | `/Library/Application Support/SLogMetaRaw/lib/slogmetaraw` |
| Script do menu | `…/DaVinci Resolve/Fusion/Scripts/Utility/Log MetaRaw.py` |
| Cache por clipe (JSON) | `~/Library/Application Support/SLogMetaRaw/cache` |

**Requisitos:** macOS 12 ou posterior, Apple silicon ou Intel, DaVinci Resolve 21 ou 20 (no Resolve 20: instale o Python 3 em python.org, ele não traz nenhum próprio).
**Clipes:** Sony XAVC em `.MP4` ou `.MXF`, mais as câmeras de [docs/CAMERAS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/CAMERAS.md) (Panasonic V-Log, Apple Log, DJI D-Log e D-Log M e outras), cada uma com a confiabilidade da sua leitura. O script lê todos. Os nós revelam **S-Log3** (S-Gamut3.Cine ou S-Gamut3), **S-Log2**, **S-Log** (S-Gamut) e as curvas dessas câmeras. Desde a 3.2.0 o DJI D-Log M usa a curva medida numa Osmo Action 6 ([docs/DJI_LOG.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DJI_LOG.md)): para as outras câmeras DJI é a melhor estimativa, não uma calibração. Com um perfil desconhecido ficam neutros e avisam.

---

## 3. Em resumo

1. Importe o material. Abra **Workspace › Scripts › Log MetaRaw**, clique em **1 · Ler metadados** e depois em **2 · Gravar no Resolve**.
2. Na página Color, adicione o **Log MetaRaw** como **primeiro nó**. Ele parte do EI, dos Kelvin e do tint do clipe: nesses valores não muda nada.
3. Corrija o balanço de branco e a exposição no nó, usando as vistas de falsa cor.
4. Molde os tons com **Toni** (Tons). Para o visual de um filme, um processo antigo, uma fita ou uma câmera de época,
   acrescente o **Log MetaRaw Creative** como nó seguinte.
5. Depois o resto da correção e, por último, seu CST / LUT / DRT de saída.

```
Log MetaRaw  →  Log MetaRaw Creative  →  resto da correção  →  CST / LUT / DRT de saída
```

![Recommended node tree](images/08-node-tree.png)

> Copie o nó para outro clipe e ele se reinicia com os dados desse clipe.

---

## 4. O script

**Workspace › Scripts › Log MetaRaw**

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

## 5. O nó Log MetaRaw

**Color › OpenFX › Log MetaRaw** — primeiro nó, antes de qualquer CST ou LUT.

![OpenFX library with both nodes](images/07-openfx-library.png)

O nó é **pontual**: cada pixel depende só de si mesmo. Nunca cria halos, e o **Generate LUT** pode exportá-lo (recomendados 65 pontos). As configurações são salvas por clipe.

> Os painéis dos nós estão em italiano. Os rótulos abaixo vêm com tradução.

![Main node panel — top](images/09-node-panel-top.png)

| Controle | O que faz |
|---|---|
| **Versão** (no topo) | Mostra `v3.2.0`. Uma vez por dia pergunta ao GitHub pela última release; se houver uma, mostra **🟢 v3.2.0 → 3.x.y** e um clique abre o download do instalador do seu sistema (`.dmg`, `.exe`, `.deb`/`.rpm`/`.run`). Nunca instala nada sozinho. |
| **Camera** · **Rileggi metadata** (reler) | A câmera que foi lida. *Rileggi* relê o clipe, devolve cada controle aos valores da câmera e grava os metadados do clipe no Media Pool. Responde em cerca de 2 s. |
| **Ripresa** (gravação) · **Decodifica** | *Clip* permite mudar os controles; *Camera metadata* trava-os nos valores de gravação |
| **Bilanciamento** (balanço) · **Temperatura** · **Tint** | As shot ou um preset. Adaptação cromática Bradford em luz linear, a partir do branco registrado pela câmera |
| **Punto bianco** (ponto branco) | o conta-gotas do Resolve: escolha na imagem um ponto que deve ser neutro e Temperatura e Tint se ajustam sozinhos, com a mesma matemática do render |
| **Esposizione** (EI) | índice de exposição: o dobro do EI = +1 stop |
| **False color** | uma caixa acima de Temperatura, uma abaixo de Tint e uma abaixo de Esposizione; veja abaixo |
| **Spazio colore** (espaço de cor) | três subgrupos. **Ingresso** (entrada): Spazio colore e Gamma que entram no nó, Rilevato (detectado) e *Ripristina da camera*, Data level, Livello e *Ripristina da clip*. **Senza metadata**: *Sblocca controlli* e as referências de gravação. **Uscita** (saída): **Tone mapping** (Nessuno/CST, ACES 2.0, Luminanza, Filmico, ACES 1.3, OpenDRT), Compressione e Mappatura gamut, Vibrance luci, Spazio colore e Gamma (como um Color Space Transform; *Timeline* não converte), tamanho e exportação da LUT |
| **Toni** (tons) | Highlights, **Bianco** (stop), Soft Clip, Saturation (−100…+100). *Mostra tutti i controlli* acrescenta Contrast, Shadows, Whites, Blacks, Vibrance e as **Zone** (Black, Shadow, Light, Specular com Exp, Sat, Range e Falloff, Contrast Pivot, false color *Zone*); aparecem sozinhos se o nó os usa |
| **Dati di ripresa** (Dados de gravação) | Só leitura: objetiva, focal, abertura, foco, obturador, EI, WB, fps, ND, LUT da câmera. |

![Avanzate and Dati di ripresa](images/14-avanzate-dati.png)

**Spazio colore em detalhe**

- **Ingresso › Spazio colore / Gamma** (entrada do nó) — *Automatico* pergunta ao Resolve ou lê a câmera; *Ripristina da camera* os traz de volta. **Rilevato** mostra o que o nó lê.
- **Ingresso › Data level** — corrige a escala de code values quando o Resolve decodifica um clipe na errada. **Livello** mostra a lida, *Ripristina da clip* volta a Automatico. O *Corrigir o Data Level* do script corrige isso para o projeto inteiro.
- **Ingresso › Stato** (estado) — o que o nó detectou; também avisa quando uma vista de falsa cor está ativa.
- **Senza metadata › Sblocca controlli** — veja [§11](#11-casos-especiais).

> **A saída Rec.709** no nó com Tone mapping *Nessuno (CST)* é um CST simples (as altas luzes além de Bianco clipam), e qualquer conversão de saída exclui o nó Creative. Prefira um CST de saída no final da árvore.

**Tone mapping com saída de cena (3.2.0).** Com um rendering (ACES 2.0, Filmico, OpenDRT…), um Spazio colore e uma Gamma de cena como DaVinci WG / DaVinci Intermediate não são mais substituídos por Rec.709 / Gamma 2.4: a imagem renderizada para Rec.709 viaja nessa codificação, para passá-la a outros plugins (Dehancer…), e um CST para Rec.709 / Gamma 2.4 sem tone mapping a devolve idêntica. As saídas de tela (Rec.709, P3, Rec.2020) não mudam.

---

## 6. Falsa cor

Uma vista por controle, posicionada ao lado do controle deslizante a que serve (acima de Temperatura, abaixo de Tint e de Esposizione). **A vista substitui a imagem — desligue-a antes de renderizar.** (Um plugin OpenFX não consegue desenhar uma sobreposição no visor do Resolve.)

Primeiro, coloque *Decodifica* em **Clip**: em *Camera metadata* os controles ficam travados.

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

> **O custo honesto de um nó pontual:** o que ele comprime, comprime também a textura ali dentro.

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

## 8. O nó Log MetaRaw Creative

Um único motor que refaz **o processo**, não uma imitação do resultado. A luz da cena, decodificada em Rec.2020 linear,
vira até três **registros** com a sua sensibilidade espectral. Cada um é exposto na curva do material e transformado em
imagem como aquele processo fazia: corantes subtrativos, filtros aditivos, um depósito de prata ou platina no papel, um
sinal de vídeo. Cada processo é uma tabela de números e o painel mostra só os controles do processo escolhido.

| Processo | O que modela |
|---|---|
| **Negativo colorido e cópia** | três camadas com contraste, pé e ombro próprios, interimagem, grão por camada, halo, cópia positiva com luzes R G B; 14 filmes, também do Leste (Sovcolor, ORWO, Svema, Gevacolor). *Saída Log* dá o escaneamento do negativo |
| **Reversível**, **Preto e branco** | diapositivo; P&B com sensibilidade azul, orto ou pan e filtro de tomada compensado |
| **Technicolor** três tiras e duas cores | três negativos do prisma, matrizes por embebição, chave K até 1944; só vermelho e verde |
| **Autochrome**, **Dufaycolor** | mosaico aditivo de grãos de fécula tingidos ou retícula regular, visto por transparência |
| **Três chapas**, **Kinemacolor** | exposições em sequência: com *Intervallo* > 0 o nó lê os quadros seguintes e o que se move se separa em cores (Prokudin-Gorski) |
| **Daguerreótipo**, **Colódio**, papel salgado e albumina, cianotipia, platina | depósitos sensíveis ao azul e ao UV, espelho e oxidação, suporte, borda, viragem |
| **Vídeo analógico** | 11 formatos, do broadcast ao VHS LP e Betacam SP: banda de luma e cor, atraso da cor, peaking, ruído, troca de cabeças, dropouts |
| **Digital de época** | 10 câmeras: clip e knee, nitidez eletrônica, ruído, subamostragem, banding |

- Vai **logo depois** do Log MetaRaw. Decodifica em luz linear e sai em Rec.709 · Gamma 2.4 (a cópia projetada), ou na
  codificação de entrada com *Saída Log*.
- É **espacial**: o Generate LUT o exclui. Os raios seguem a altura do quadro. Ruído determinístico: CPU e Metal dão a
  mesma imagem.
- Substitui o nó Detail no mesmo lugar: uma correção salva com o Detail abre como um Creative novo.

Os números são estimativas da pesquisa em [docs/CREATIVE.md](docs/CREATIVE.md), não perfis de produtos: precisam ser
ajustados com material real. Ainda não testado no Resolve; Metal verificado pelos testes da CI do macOS.

**Novidades da 4.3.1.**

- **Pelo na janela, poeira e riscos reais**: o pelo afina para a ponta, tem densidade irregular e uma sombra
  desfocada como no filme (mais no Super 8 e nas cópias); às vezes é encaracolado ou uma fibra de dois fios. A poeira
  tem bordas irregulares e grãos de todo tamanho; os riscos mudam de espessura e intensidade.
- **Digitalização com as marcas do fabricante**: de borda a borda a margem traz a marca (EASTMAN antes de 1990,
  depois KODAK; FUJI, AGFA, ORWO…), o código do filme naquele formato, NITRATE FILM, SAFETY FILM ou ESTAR, um número
  progressivo e desde 1990 o KeyKode com código de barras; filmes de foto numeram os fotogramas. A luz do scanner
  brilha em volta das perfurações e no corte da base. Novo controle *Overscan*.

**Novidades da 4.3.0.**

- **Películas por formato**: com uma câmera escolhida aparecem só as películas feitas no seu formato, de todos os
  fabricantes e com o código desse formato (em 16 mm a 500T é 7219); as listas completas ficam a uma caixa.
- **Digitalização**: o quadro, o quadro com as bordas da janela ou a película de borda a borda com perfurações, linhas
  entre quadros e quadros vizinhos, com as medidas reais de cada formato. Janela quadrada, arredondada ou limada aberta.
- **O negativo como é**: o negativo colorido contra a luz, complementar sobre a máscara laranja, com o seu grão.
- **Suporte da película**: nitrato, acetato ou poliéster (ou por época e formato) e a sua degradação: manchas âmbar do
  nitrato, cristais e instabilidade do acetato.
- **Oscilação realista** na filmagem, na cópia e na projeção, cada uma com a sua caixa; **pelo na janela** conforme a
  câmera.
- **Consoles** como meio de visualização: NES, Master System, Mega Drive, Super Nintendo, PlayStation e Saturn numa TV
  de tubo.
- Corrigido o fecho ao escolher os meios de visualização adicionados na 4.2.0.

**Novidades da 4.2.0.**

- **Dispositivo › modelo**: parte-se da máquina (câmera de cinema, videocâmera, cinema digital, telefone), que carrega
  formato, fita, sensor e defeitos; depois fabricante e filme, com nomes reais. Os interruptores *Tom*, *Cor* e
  *Textura* permitem usar, por exemplo, só o grão no seu próprio vídeo.
- **Vídeo emulado**: fita VHS e Betacam refeita linha a linha (ruído FM, cor atrasada, cabeças, dropouts) e sensor da
  câmera: tubos com rastro e caudas de cometa, smear do CCD, falsa cor dos tubos de listras.
- **Digitais por modelo**: tamanho do sensor e aliasing, nitidez interna, blocos 8x8 do codec e cor subamostrada (do
  Motorola RAZR à ARRI Alexa).
- **Copiado para**: a cópia em VHS, Betacam, VHS de terceira geração, cinescópio 16 mm ou film-out 35 mm, grão sobre
  grão.
- **Lente**: 14 objetivas (Cooke, Zeiss Super Speed, Canon K35, Panavision, LOMO, Helios, Petzval…) com distorção,
  bordas suaves, redemoinho, véu e faixa anamórfica.
- **Novos meios de exibição**: projetor Super 8, Trinitron, plasma, primeiro LCD.

---

## Os nós Noise, Stabilize e Motion Blur (4.0.0)

Trabalham sobre a imagem da câmera, antes do **Log MetaRaw**.

- **Log MetaRaw Noise** sabe quanto ruído esperar e onde. Do registro do script lê a câmera, o ISO (ou o EI e o ISO
  nativo), o codec, os bits por pixel e o 4:2:0. Tem perfis para Sony FX6 e FX30, iPhone 17 Pro e DJI Osmo Action 4
  e 5 Pro. Também pode medir o ruído num quadro com zonas planas.
  - *Temporal*: de ±1 a ±3 quadros vizinhos, alinhados por blocos e ponderados pixel a pixel, para que as partes em
    movimento e os cortes não borrem.
  - *Espacial*: luminância e cor separadas, em 5 bandas wavelet. Cada coeficiente é comparado com a energia da sua
    vizinhança, o que mantém a textura.
  - Vistas do ruído removido, proteção de detalhe e pele, e **Salta Noise ovunque** para desligar todos os nós Noise
    enquanto edita.
  - No Mac tudo roda na GPU Metal, e os testes verificam que dá o mesmo resultado da CPU. Nos outros sistemas roda na
    CPU.
  - Os perfis são estimativas até serem calibrados com amostras reais. [docs/DENOISE.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DENOISE.md)
- **Stabilize** lê o giroscópio do clipe (Gyroflow integrado). Tem estilos do micromovimento à grua e um **MASTER**
  animável. **Motion Blur** reconstrói o obturador a partir do fluxo óptico, por exemplo de 50p a 90° para 25p a
  180°. Ambos vêm do Mecena Motion (`motion/`) e são instalados pelo pacote macOS, só Apple silicon.
- **Aquisição profunda**, uma caixa na janela do script: grava o giroscópio e a objetiva de cada quadro na
  MecenaClipDB, para o Stabilize.

Nenhum dos três nós foi ainda testado no Resolve com clipes reais.

## Log MetaRaw Audio (VST3, 4.2.0)

O gêmeo sonoro do Creative, para o **Fairlight** (Mac e Windows): dois plugins num só módulo, `Log MetaRaw Audio.vst3`.

- **Ripresa** (gravação), nas faixas: gravador (Geloso G.257, Nagra, Uher, Studer, Revox, Walkman, Auricon, fonógrafo
  Edison, gravador de fio…), microfone (o de série ou RCA 44, U 47, C12, SM58, MKH 416…), suporte e redução de ruído.
- **Mix e supporto** (mixagem e suporte), em buses e master: console (RCA, EMI REDD.51, Neve, SSL, Mackie), suporte
  (fita, cassete, 78/45/33 rpm, VHS, trilha ótica, rádio, telefone), cópias e escuta (rádio valvulado, jukebox, TV,
  cinema).
- Cada estágio tem um interruptor; Saturação, Ruído, Wow e flutter, Dry/wet e Saída ajustam o resultado.
- Nos instaladores é um componente à parte, marcado por padrão. Linux: o Resolve não carrega VST3 nativos.

Os valores são estimativas de especificações e descrições de época ([docs/audio/AUDIO.md](docs/audio/AUDIO.md)); ainda
não testado no Fairlight.

## 9. Pipelines: onde cada nó vai

### A — Corrigir em DaVinci Wide Gamut (recomendado)

```
Log MetaRaw          Color Space/Gamma: DaVinci WG · DaVinci Intermediate
  → sua correção (DWG)
  → CST de saída        DaVinci WG/Intermediate → Rec.709 · Gamma 2.4, tone mapping do DaVinci
```

Com tone mapping depois, coloque **Bianco entre 4 e 5**. O **Creative**, se usar, vai logo depois do Log MetaRaw e sai em Rec.709 · Gamma 2.4 (ou na codificação de entrada com *Uscita Log*): numa correção DWG coloque-o por último, no lugar do CST de saída.

### B — Tudo em log

```
Log MetaRaw          Color Space/Gamma: Timeline
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
| Rosto em contraluz na sombra | Main › Toni | Shadows +30…+60, Blacks −20…−40 |
| Visual mais de "película" | Main › Toni | Contrast +20…+30, Highlights −60 |
| Sombras ruidosas e saturadas | Main › Zone | Shadow Sat −30…−50 |
| Reflexos especulares | Main › Zone | Specular Exp −1…−2 |

---

## 11. Casos especiais

**ProRes de um gravador externo / clipe ilegível.** O nó fica neutro. Marque **Spazio colore › Senza metadata › Sblocca controlli** e digite o EI, os Kelvin e o tint de gravação: os controles se ativam e o nó começa neutro.

**Perfis não log** (Cine, HLG, S-Cinetone): os nós ficam neutros e avisam isso em *Stato*.

**Data Level errado.** Se os pretos parecem levantados ou esmagados direto da câmera, o Resolve está decodificando na escala de code values errada. Rode o script com *Corrigir o Data Level* ativado (correção para o projeto inteiro), ou use *Spazio colore › Ingresso › Data level* num único clipe. Aprofundamento (em italiano): [docs/DATA_LEVELS.md](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/docs/DATA_LEVELS.md)

**Câmeras sem Kelvin** (por exemplo, a a6300): o Kelvin é estimado a partir do preset de luz e sinalizado.

**Desempenho.** Abrir um projeto não lê nenhum arquivo. Abrir o painel espera no máximo meio segundo; um disco lento termina em segundo plano, com limite de 15 s.

---

## 12. Atualizações e privacidade

- Os **nós** perguntam ao GitHub pela última release no máximo uma vez por dia, em segundo plano. A requisição leva só a versão (`User-Agent: SLogMetaRaw/3.0.0`).
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

Dê um duplo clique em **Disinstalla Log MetaRaw.command** no DMG (na primeira vez: botão direito › Abrir). Ele:

1. lista tudo o que vai remover (todas as versões 1.x e 2.x, o nome antigo *SonyMeta*, instalações de desenvolvimento, cache, configurações, logs);
2. pede para você fechar o Resolve;
3. pergunta se deve apagar também os CSVs exportados;
4. verifica que nada ficou para trás.

Execute com `--dry-run` para ver o que seria removido sem tocar em nada. Os metadados já gravados nos projetos do Resolve permanecem — fazem parte dos projetos.

![Uninstaller in Terminal](images/20-uninstaller.png)

---

## 14. Solução de problemas / FAQ

**O nó Creative está faltando / o painel parece antigo.**
O Resolve está usando seu cache de plugins antigo. Saia do Resolve, apague o `OFXPluginCacheV2.xml` (o instalador normalmente faz isso), reinicie.

**O nó não faz nada.**
Isso é esperado nos valores de gravação. Verifique se *Decodifica* está em **Clip**, e leia *Spazio colore › Ingresso › Stato*: ele diz se o perfil não é log ou se faltam metadados.

**Minha renderização saiu com falsa cor.**
Uma vista ficou ativada. Desligue todas as vistas de falsa cor antes de renderizar.

**As altas luzes parecem comprimidas duas vezes / sem vida.**
Você tem um DRT (ACES, AgX, tone mapping do DaVinci) depois do nó: suba **Bianco** para 4–5.

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

- O painel Camera Raw do Resolve e a estabilização por giroscópio não podem ser desbloqueados para MP4: vivem dentro dos decodificadores do Resolve. O Log MetaRaw reconstrói os controles de cor; não abre uma porta dentro do Resolve.
- O S-Log2 segue o documento da Sony; a curva S-Log2 do Resolve difere em cerca de 0,15 stop.
- Ainda por verificar em mais arquivos: XAVC HS (HEVC), HLG, S-Cinetone, zooms motorizados.
- Testado apenas no Resolve Studio 21.1 no macOS. As builds Linux e Windows são só CPU e não testadas dentro do Resolve (veja [§15](#15-linux-e-windows-experimental)).
- Projeto independente e amador, fornecido como está, sem garantia e sem responsabilidade pelo uso profissional. Não afiliado nem endossado pela Sony ou pela Blackmagic Design.

---

**Ivan Mazzone + Claude** · [github.com/ivan-94m](https://github.com/ivan-94m) · [@ivan_94m](https://instagram.com/ivan_94m) · [GNU GPL v3.0 ou posterior](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/LICENSE) · [Notas de versão](https://github.com/Mecena-SRL/SLogMetaRaw/blob/main/RELEASE_NOTES.md)
