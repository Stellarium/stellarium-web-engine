// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<v-dialog scrollable max-width='600' v-model="$store.state.showDataCreditsDialog">
  <v-card v-if="$store.state.showDataCreditsDialog">
    <v-card-title><div class="text-h5">Data Credits</div></v-card-title>
    <v-card-text style="height: 600px;">
      <h3>Stars</h3>
      <p>Combination of the following catalogues:
        <ul class="data-credits">
          <li>Gaia DR2: <i>This work has made use of data from the European Space Agency (ESA) mission Gaia (<a href="https://www.cosmos.esa.int/gaia" target="_blank" rel="noopener">https://www.cosmos.esa.int/gaia</a>), processed by the Gaia Data Processing and Analysis Consortium (DPAC, <a href="https://www.cosmos.esa.int/web/gaia/dpac/consortium" target="_blank" rel="noopener">https://www.cosmos.esa.int/web/gaia/dpac/consortium</a>). Funding for the DPAC has been provided by national institutions, in particular the institutions participating in the Gaia Multilateral Agreement.</i>
          <br>Gaia Collaboration et al. (<a href="https://gea.esac.esa.int/archive/documentation/GDR2/bib.html#bib173" target="_blank" rel="noopener">2016</a>): Description of the Gaia mission (spacecraft, instruments, survey and measurement principles, and operations)
          <br>Gaia Collaboration et al. (<a href="https://gea.esac.esa.int/archive/documentation/GDR2/bib.html#bib15" target="_blank" rel="noopener">2018b</a>): Summary of the contents and survey properties.
          </li>
          <li>Hipparcos: The Hipparcos star catalog From ESA (European Space Agency) and the Hipparcos mission.<br>
          ref. ESA, 1997, The Hipparcos and Tycho Catalogues, ESA SP-1200
          <a href="http://cdsweb.u-strasbg.fr/ftp/cats/I/239" target="_blank" rel="noopener">http://cdsweb.u-strasbg.fr/ftp/cats/I/239</a></li>
          <li>Brigh Stars Catalogue: Bright Star Catalogue, 5th Revised Ed. (Hoffleit+, 1991)<br>
          <a href="http://cdsarc.u-strasbg.fr/ftp/pub/cats/V/50/" target="_blank" rel="noopener">http://cdsarc.u-strasbg.fr/ftp/pub/cats/V/50/</a></li>
        </ul>
      </p>
      <h3>Deep Sky Objects</h3>
      <p>Combination of the following catalogues:
        <ul class="data-credits">
          <li>HyperLeda database, <i>Makarov et al.</i> <a href="http://adsabs.harvard.edu/abs/2014A%26A...570A..13M" target="_blank" rel="noopener">2014, A&A, 570, A13</a>: <a href="http://leda.univ-lyon1.fr" target="_blank" rel="noopener">http://leda.univ-lyon1.fr</a>.</li>
          <li>Queries from Simbad, especially for the objects cross matching: <i>This research has made use of the SIMBAD database,
          operated at CDS, Strasbourg, France </i><br>
          <a href="http://adsabs.harvard.edu/abs/2000A%26AS..143....9W" target="_blank" rel="noopener">2000,A&AS,143,9</a>, "The SIMBAD astronomical database", Wenger et al.</li>
          <li>Open NGC Database: Mattia Verga <a href="https://github.com/mattiaverga/OpenNGC" target="_blank" rel="noopener">https://github.com/mattiaverga/OpenNGC</a></li>
          <li>Caldwell Catalogue: <a href="https://fr.wikipedia.org/wiki/Liste_des_objets_de_Caldwell" target="_blank" rel="noopener">from wikipedia</a></li>
          <li>Descriptions and some data taken from wikipedia</li>
        </ul>
      </p>
      <h3>Background Image</h3>
      <p>Digitized Sky Survey:
        <ul class="data-credits">
          <li>STScI/NASA <a href="http://archive.stsci.edu/dss/copyright.html" target="_blank" rel="noopener">http://archive.stsci.edu/dss/copyright.html</a>, Colored & Healpixed by CDS. This HiPS survey is based on 2 others HiPS surveys, respectively DSS2-red and DSS2-blue HiPS, both of them directly generated from original scanned plates downloaded from STScI site. The red component has been built from POSS-II F, AAO-SES,SR and SERC-ER plates. The blue component has been build from POSS-II J and SERC-J,EJ. The green component is based on the mean of other components. Three missing plates from red survey (253, 260, 359) has been replaced by pixels from the DSSColor STScI jpeg survey. The 11 missing blue plates (mainly in galactic plane) have not been replaced (only red component).</li>
          <li><i>The Digitized Sky Surveys were produced at the Space Telescope Science Institute under U.S. Government grant NAG W-2166. The images of these surveys are based on photographic data obtained using the Oschin Schmidt Telescope on Palomar Mountain and the UK Schmidt Telescope. The plates were processed into the present compressed digital form with the permission of these institutions. The National Geographic Society - Palomar Observatory Sky Atlas (POSS-I) was made by the California Institute of Technology with grants from the National Geographic Society. The Second Palomar Observatory Sky Survey (POSS-II) was made by the California Institute of Technology with funds from the National Science Foundation, the National Geographic Society, the Sloan Foundation, the Samuel Oschin Foundation, and the Eastman Kodak Corporation. The Oschin Schmidt Telescope is operated by the California Institute of Technology and Palomar Observatory. The UK Schmidt Telescope was operated by the Royal Observatory Edinburgh, with funding from the UK Science and Engineering Research Council (later the UK Particle Physics and Astronomy Research Council), until 1988 June, and thereafter by the Anglo-Australian Observatory. The blue plates of the southern Sky Atlas and its Equatorial Extension (together known as the SERC-J), as well as the Equatorial Red (ER), and the Second Epoch [red] Survey (SES) were all taken with the UK Schmidt. Supplemental funding for sky-survey work at the ST ScI is provided by the European Southern Observatory.</i></li>
        </ul>
      </p>
      <h3>Planets Image</h3>
      <p>All images from NASA & JPL under public domain license: <a href="http://www.jpl.nasa.gov/images/policy/index.cfm" target="_blank" rel="noopener">http://www.jpl.nasa.gov/images/policy/index.cfm</a></p>
      <p><br>
      </p>
      <h3>Minor Planets</h3>
      <p>All data comes from the IAU Minor Planet Center <a href="https://www.minorplanetcenter.net/data" target="_blank" rel="noopener">https://www.minorplanetcenter.net/data</a>. <i>This research has made use of data and/or services provided by the International Astronomical Union's Minor Planet Center.</i></p>
      <h3>Others</h3>
      <p>Landscape images by Fabien Chereau</p>
      <p>Constellation lines by Fabien Chereau</p>
      <p>All other graphics by <a href="https://noctua-software.com" target="_blank" rel="noopener">Noctua Software</a></p>
    </v-card-text>
    <v-card-actions>
      <v-spacer></v-spacer><v-btn text class="blue--text darken-1" @click.native="$store.state.showDataCreditsDialog = false">Close</v-btn>
    </v-card-actions>
  </v-card>
</v-dialog>
</template>

<script>
</script>

<style>
.data-credits {
  padding-left: 30px;
}
a {
  color: #82b1ff;
}
</style>
