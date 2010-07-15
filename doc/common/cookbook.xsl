<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:include href="ref-html-style.xsl"/>

  <xsl:template match="inlinemediaobject" priority="100">
    <p>
      <video controls="controls">
        <xsl:attribute name="src"><xsl:value-of select="videoobject/videodata/@fileref"/></xsl:attribute>
        <!-- fallback link to video for non-HTML 5 browsers -->
        <a>
          <xsl:attribute name="href">
            <xsl:value-of select="videoobject/videodata/@fileref"/>
          </xsl:attribute>
          <xsl:apply-templates select="alt"/>
        </a>
      </video>
    </p>
  </xsl:template>

</xsl:stylesheet>
