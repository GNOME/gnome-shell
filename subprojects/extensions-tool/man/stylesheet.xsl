<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		version='1.0'>
<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"/>

<xsl:template match="variablelist/title">
  <xsl:text>.PP&#10;</xsl:text>
  <xsl:call-template name="bold">
    <xsl:with-param name="node" select="."/>
    <xsl:with-param name="context" select=".."/>
  </xsl:call-template>
  <xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="varlistentry[preceding-sibling::title]">
  <xsl:if test="not(preceding-sibling::varlistentry)">
    <xsl:text>.RS 4&#10;</xsl:text>
    <!-- comment out the leading .PP added by the original template -->
    <xsl:text>.\"</xsl:text>
  </xsl:if>
  <xsl:apply-imports/>
  <xsl:if test="position() = last()">
    <xsl:text>.RE&#10;</xsl:text>
  </xsl:if>
</xsl:template>

</xsl:stylesheet>
