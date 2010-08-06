<?xml version="1.0"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
  <xsl:include href="ref-html-style.xsl"/>

  <!-- Make the ToC be of depth 2 -->
  <xsl:param name="toc.max.depth" select="2"/>

  <!-- Strip leading and trailing space from text children of programlisting -->

  <xsl:template match="programlisting/text()">
    <xsl:variable name="before" select="preceding-sibling::node()"/>
    <xsl:variable name="after" select="following-sibling::node()"/>

    <xsl:variable name="conts" select="."/>

    <xsl:variable name="contsl">
      <xsl:choose>
        <xsl:when test="count($before) = 0">
          <xsl:call-template name="remove-lf-left">
            <xsl:with-param name="astr" select="$conts"/>
          </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$conts"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:variable name="contslr">
      <xsl:choose>
        <xsl:when test="count($after) = 0">
          <xsl:call-template name="remove-ws-right">
            <xsl:with-param name="astr" select="$contsl"/>
          </xsl:call-template>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="$contsl"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>

    <xsl:value-of select="$contslr"/>

  </xsl:template>

  <!-- eats linefeeds from the left -->
  <xsl:template name="remove-lf-left">
    <xsl:param name="astr"/>

    <xsl:choose>
      <xsl:when test="starts-with($astr,'&#xA;') or
                      starts-with($astr,'&#xD;')">
        <xsl:call-template name="remove-lf-left">
          <xsl:with-param name="astr" select="substring($astr, 2)"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$astr"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <!-- eats whitespace from the right -->
  <xsl:template name="remove-ws-right">
    <xsl:param name="astr"/>

    <xsl:variable name="last-char">
      <xsl:value-of select="substring($astr, string-length($astr), 1)"/>
    </xsl:variable>

    <xsl:choose>
      <xsl:when test="($last-char = '&#xA;') or
                      ($last-char = '&#xD;') or
                      ($last-char = '&#x20;') or
                      ($last-char = '&#x9;')">
        <xsl:call-template name="remove-ws-right">
          <xsl:with-param name="astr"
            select="substring($astr, 1, string-length($astr) - 1)"/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$astr"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

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
